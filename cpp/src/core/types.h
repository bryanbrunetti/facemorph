#pragma once

#include <opencv2/core.hpp>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>
#include <array>

namespace core {

/// Five facial keypoints (eye centers, nose tip, mouth corners).
struct FaceKeypoints {
    cv::Point2f left_eye;
    cv::Point2f right_eye;
    cv::Point2f nose;
    cv::Point2f mouth_left;
    cv::Point2f mouth_right;

    std::vector<cv::Point2f> toVector() const {
        return {left_eye, right_eye, nose, mouth_left, mouth_right};
    }
};

/// Result of aligning a single face image.
struct AlignedFace {
    cv::Mat     image;       ///< BGR aligned image
    FaceKeypoints keypoints; ///< 5 keypoints in aligned-image space
    std::string filename;    ///< Original filename (for logging)
};

/// Landmark extraction result.
struct LandmarkResult {
    std::vector<cv::Point2f> points;  ///< Landmarks + 8 boundary points
    bool used_dlib;                    ///< true = 68-pt dlib, false = 5-pt fallback
};

/// Settings for the processing pipeline.
struct PipelineSettings {
    std::filesystem::path input_dir;
    std::filesystem::path output_path;
    std::filesystem::path model_path;  ///< Path to shape_predictor_68_face_landmarks.dat
    double fps         = 24.0;
    int    duration_ms = 0;    ///< Hold time in ms (0 = one frame per image)
    int    morph_steps = 0;    ///< Transition frames between images (0 = cut)
    int    width       = 0;    ///< Output width  (0 = auto from first image)
    int    height      = 0;    ///< Output height (0 = auto from first image)
    bool   save_frames = false;
};

/// Progress callback: (current_step, total_steps, message).
using ProgressCallback = std::function<void(int, int, const std::string&)>;

} // namespace core