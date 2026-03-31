#pragma once

#include <opencv2/core.hpp>
#include <vector>
#include <array>

namespace core {

/// Compute a Delaunay triangulation over a set of 2D points.
/// Returns a list of index triples (i, j, k) into the input points vector.
/// Points are clamped to stay within the image bounds.
std::vector<std::array<int, 3>> computeDelaunay(
    const std::vector<cv::Point2f>& points,
    cv::Size size);

} // namespace core