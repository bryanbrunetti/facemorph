#pragma once

#include <opencv2/core.hpp>
#include <vector>
#include <array>

namespace core {

/// Warp a single triangle from src into dst in-place.
/// srcTri and dstTri each contain exactly 3 vertices.
/// The function computes the affine transform from srcTri to dstTri,
/// warps the corresponding region of src, and blends it into dst
/// using a convex-polygon mask.
void warpTriangle(const cv::Mat& src,
                  const std::vector<cv::Point2f>& srcTri,
                  const std::vector<cv::Point2f>& dstTri,
                  cv::Mat& dst);

/// Produce a single morphed frame at blend position t (0→img1, 1→img2).
/// Both images must be the same size (BGR, CV_8UC3).
/// pts1/pts2 are corresponding landmark sets; triangles indexes into them.
/// Returns a CV_8UC3 image.
cv::Mat morphPair(const cv::Mat& img1, const cv::Mat& img2,
                  const std::vector<cv::Point2f>& pts1,
                  const std::vector<cv::Point2f>& pts2,
                  const std::vector<std::array<int, 3>>& triangles,
                  float t);

/// Simple alpha cross-dissolve fallback.
/// Returns (1-t)*img1 + t*img2, both images must be the same size and type.
cv::Mat crossfadePair(const cv::Mat& img1, const cv::Mat& img2, float t);

} // namespace core