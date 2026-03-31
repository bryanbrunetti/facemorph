#include "delaunay.h"

#include <opencv2/imgproc.hpp>
#include <cmath>
#include <limits>

namespace core {

std::vector<std::array<int, 3>> computeDelaunay(
    const std::vector<cv::Point2f>& points,
    cv::Size size)
{
    const int w = size.width;
    const int h = size.height;

    if (points.empty() || w <= 0 || h <= 0) {
        return {};
    }

    // Create subdivision with the image bounding rectangle.
    cv::Rect rect(0, 0, w, h);
    cv::Subdiv2D subdiv(rect);

    // Insert each point, clamped to [1, w-2] x [1, h-2] to stay safely
    // inside the bounding rectangle (Subdiv2D rejects points on the border).
    for (const auto& p : points) {
        float x = std::min(std::max(1.0f, std::round(p.x)), static_cast<float>(w - 2));
        float y = std::min(std::max(1.0f, std::round(p.y)), static_cast<float>(h - 2));
        subdiv.insert(cv::Point2f(x, y));
    }

    // Retrieve the raw triangle list (each triangle = 6 consecutive floats).
    std::vector<cv::Vec6f> triangleList;
    subdiv.getTriangleList(triangleList);

    // Pre-convert input points to a contiguous structure for distance lookups.
    const int nPts = static_cast<int>(points.size());

    std::vector<std::array<int, 3>> triangles;
    triangles.reserve(triangleList.size());

    for (const auto& t : triangleList) {
        // Three vertices of the triangle.
        float vx[3] = {t[0], t[2], t[4]};
        float vy[3] = {t[1], t[3], t[5]};

        // Skip triangles with any vertex outside the image bounds.
        bool outOfBounds = false;
        for (int v = 0; v < 3; ++v) {
            if (vx[v] < 0.0f || vx[v] >= static_cast<float>(w) ||
                vy[v] < 0.0f || vy[v] >= static_cast<float>(h)) {
                outOfBounds = true;
                break;
            }
        }
        if (outOfBounds) continue;

        // Map each vertex back to the nearest input point index.
        std::array<int, 3> indices{};
        bool valid = true;
        for (int v = 0; v < 3; ++v) {
            float bestDist = std::numeric_limits<float>::max();
            int bestIdx = -1;
            for (int i = 0; i < nPts; ++i) {
                float dx = points[i].x - vx[v];
                float dy = points[i].y - vy[v];
                float dist = dx * dx + dy * dy;
                if (dist < bestDist) {
                    bestDist = dist;
                    bestIdx = i;
                }
            }
            if (bestIdx < 0) {
                valid = false;
                break;
            }
            indices[v] = bestIdx;
        }
        if (!valid) continue;

        // Only keep triangles with three distinct indices.
        if (indices[0] != indices[1] &&
            indices[1] != indices[2] &&
            indices[0] != indices[2]) {
            triangles.push_back(indices);
        }
    }

    return triangles;
}

} // namespace core