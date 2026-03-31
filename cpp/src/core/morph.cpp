#include "morph.h"

#include <opencv2/imgproc.hpp>
#include <algorithm>

namespace core {

void warpTriangle(const cv::Mat& src,
                  const std::vector<cv::Point2f>& srcTri,
                  const std::vector<cv::Point2f>& dstTri,
                  cv::Mat& dst)
{
    // Bounding rects for source and destination triangles
    cv::Rect r1 = cv::boundingRect(srcTri);
    cv::Rect r2 = cv::boundingRect(dstTri);

    if (r1.width <= 0 || r1.height <= 0 || r2.width <= 0 || r2.height <= 0)
        return;

    // Offset triangle vertices relative to their bounding rects
    std::vector<cv::Point2f> t1(3), t2(3);
    std::vector<cv::Point> t2_int(3);
    for (int i = 0; i < 3; ++i) {
        t1[i] = cv::Point2f(srcTri[i].x - static_cast<float>(r1.x),
                             srcTri[i].y - static_cast<float>(r1.y));
        t2[i] = cv::Point2f(dstTri[i].x - static_cast<float>(r2.x),
                             dstTri[i].y - static_cast<float>(r2.y));
        t2_int[i] = cv::Point(static_cast<int>(t2[i].x),
                               static_cast<int>(t2[i].y));
    }

    // Create mask for the destination triangle
    cv::Mat mask = cv::Mat::zeros(r2.height, r2.width, CV_32FC3);
    cv::fillConvexPoly(mask, t2_int, cv::Scalar(1.0f, 1.0f, 1.0f), cv::LINE_AA);

    // Crop source region with bounds clamping
    int y1 = std::max(0, r1.y);
    int x1 = std::max(0, r1.x);
    int y2_c = std::min(src.rows, r1.y + r1.height);
    int x2_c = std::min(src.cols, r1.x + r1.width);

    cv::Mat crop = src(cv::Range(y1, y2_c), cv::Range(x1, x2_c));
    if (crop.empty())
        return;

    // Resize crop if it doesn't match expected bounding rect size
    if (crop.rows != r1.height || crop.cols != r1.width) {
        cv::Mat resized;
        cv::resize(crop, resized, cv::Size(r1.width, r1.height));
        crop = resized;
    }

    // Affine warp from source triangle to destination triangle
    cv::Mat mat = cv::getAffineTransform(t1, t2);
    cv::Mat warped;
    cv::warpAffine(crop, warped, mat, cv::Size(r2.width, r2.height),
                   cv::INTER_LINEAR, cv::BORDER_REFLECT_101);

    // Blend warped triangle into destination image
    int dy  = r2.y;
    int dy2 = std::min(r2.y + r2.height, dst.rows);
    int dx  = r2.x;
    int dx2 = std::min(r2.x + r2.width, dst.cols);
    int rh  = dy2 - dy;
    int rw  = dx2 - dx;

    if (rh <= 0 || rw <= 0)
        return;

    cv::Mat region = dst(cv::Range(dy, dy2), cv::Range(dx, dx2));
    cv::Mat m = mask(cv::Range(0, rh), cv::Range(0, rw));
    cv::Mat w = warped(cv::Range(0, rh), cv::Range(0, rw));

    // dst_region = region * (1 - mask) + warped * mask
    cv::Mat inv_mask;
    cv::subtract(cv::Scalar(1.0f, 1.0f, 1.0f), m, inv_mask);

    cv::Mat blended;
    cv::multiply(region, inv_mask, blended);
    cv::Mat warped_masked;
    cv::multiply(w, m, warped_masked);
    cv::add(blended, warped_masked, region);
}

cv::Mat morphPair(const cv::Mat& img1, const cv::Mat& img2,
                  const std::vector<cv::Point2f>& pts1,
                  const std::vector<cv::Point2f>& pts2,
                  const std::vector<std::array<int, 3>>& triangles,
                  float t)
{
    // Interpolate landmark positions
    std::vector<cv::Point2f> pm(pts1.size());
    for (size_t i = 0; i < pts1.size(); ++i) {
        pm[i].x = (1.0f - t) * pts1[i].x + t * pts2[i].x;
        pm[i].y = (1.0f - t) * pts1[i].y + t * pts2[i].y;
    }

    // Convert images to float
    cv::Mat f1, f2;
    img1.convertTo(f1, CV_32FC3);
    img2.convertTo(f2, CV_32FC3);

    // Allocate morph canvases
    cv::Mat morph1 = cv::Mat::zeros(f1.size(), CV_32FC3);
    cv::Mat morph2 = cv::Mat::zeros(f2.size(), CV_32FC3);

    // Warp each triangle from both sources into the intermediate position
    for (const auto& tri : triangles) {
        int i = tri[0];
        int j = tri[1];
        int k = tri[2];

        std::vector<cv::Point2f> s1 = { pts1[i], pts1[j], pts1[k] };
        std::vector<cv::Point2f> s2 = { pts2[i], pts2[j], pts2[k] };
        std::vector<cv::Point2f> d  = { pm[i],   pm[j],   pm[k]   };

        warpTriangle(f1, s1, d, morph1);
        warpTriangle(f2, s2, d, morph2);
    }

    // Blend the two warped images
    cv::Mat blended;
    cv::addWeighted(morph1, 1.0 - static_cast<double>(t),
                    morph2, static_cast<double>(t), 0.0, blended);

    // Clamp and convert to 8-bit
    cv::Mat result;
    blended.convertTo(result, CV_8UC3);
    return result;
}

cv::Mat crossfadePair(const cv::Mat& img1, const cv::Mat& img2, float t)
{
    cv::Mat result;
    cv::addWeighted(img1, 1.0 - static_cast<double>(t),
                    img2, static_cast<double>(t), 0.0, result);
    return result;
}

} // namespace core