#include "landmarks.h"
#include <dlib/opencv.h>
#include <opencv2/imgproc.hpp>
#include <iostream>

namespace core {

LandmarkResult getLandmarks(
    const cv::Mat& aligned_bgr,
    const FaceKeypoints& fallback_kps,
    dlib::frontal_face_detector& detector,
    dlib::shape_predictor& predictor)
{
    const int h = aligned_bgr.rows;
    const int w = aligned_bgr.cols;

    // 8 boundary points (corners + edge midpoints), slightly inset
    std::vector<cv::Point2f> boundary = {
        {1.0f,              1.0f},
        {w / 2.0f,          1.0f},
        {static_cast<float>(w - 2), 1.0f},
        {1.0f,              h / 2.0f},
        {static_cast<float>(w - 2), h / 2.0f},
        {1.0f,              static_cast<float>(h - 2)},
        {w / 2.0f,          static_cast<float>(h - 2)},
        {static_cast<float>(w - 2), static_cast<float>(h - 2)},
    };

    // Convert BGR → RGB for dlib
    cv::Mat rgb;
    cv::cvtColor(aligned_bgr, rgb, cv::COLOR_BGR2RGB);
    dlib::cv_image<dlib::rgb_pixel> dlib_img(rgb);

    // Attempt face detection on the aligned image
    std::vector<dlib::rectangle> faces = detector(dlib_img);

    if (!faces.empty()) {
        // Use the first detected face rectangle
        dlib::full_object_detection shape = predictor(dlib_img, faces[0]);

        std::vector<cv::Point2f> points;
        points.reserve(68 + boundary.size());

        for (int i = 0; i < 68; ++i) {
            const dlib::point& pt = shape.part(i);
            points.emplace_back(static_cast<float>(pt.x()),
                                static_cast<float>(pt.y()));
        }

        // Append boundary points
        points.insert(points.end(), boundary.begin(), boundary.end());

        return LandmarkResult{std::move(points), true};
    }

    // Fallback: dlib missed face in aligned image, use the 5 keypoints from alignment
    std::cout << "    dlib missed face, falling back to alignment keypoints" << std::endl;

    std::vector<cv::Point2f> fallback_pts = fallback_kps.toVector();
    fallback_pts.reserve(fallback_pts.size() + boundary.size());
    fallback_pts.insert(fallback_pts.end(), boundary.begin(), boundary.end());

    return LandmarkResult{std::move(fallback_pts), false};
}

} // namespace core