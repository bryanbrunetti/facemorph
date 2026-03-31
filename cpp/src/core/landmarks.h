#pragma once

#include "types.h"
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing.h>

namespace core {

/// Extract facial landmarks from an already-aligned BGR image.
///
/// Tries dlib's 68-point predictor first (best morph quality). If dlib
/// cannot find a face in the aligned image, falls back to the 5 keypoints
/// that were already detected during alignment.
///
/// The 8 boundary points (image corners + edge midpoints) are always appended,
/// giving 76 points with dlib (68+8) or 13 points with fallback (5+8).
LandmarkResult getLandmarks(
    const cv::Mat& aligned_bgr,
    const FaceKeypoints& fallback_kps,
    dlib::frontal_face_detector& detector,
    dlib::shape_predictor& predictor);

} // namespace core