#pragma once

#include "types.h"
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing.h>
#include <optional>

namespace core {

/// Detect the primary face in a BGR image, align it so the eyes are horizontal
/// and centered, and return the aligned image along with 5 keypoints transformed
/// into the aligned coordinate space.
///
/// Uses dlib's HOG frontal_face_detector + shape_predictor (68-point) to find
/// the face and derive eye centers, nose tip, and mouth corners.
///
/// Returns std::nullopt if no face is detected.
std::optional<AlignedFace> alignFace(
    const cv::Mat& bgr_image,
    const std::string& filename,
    dlib::frontal_face_detector& detector,
    dlib::shape_predictor& predictor);

} // namespace core