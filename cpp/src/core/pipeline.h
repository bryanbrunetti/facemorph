#pragma once

#include "types.h"
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing.h>
#include <string>

namespace core {

/// Main processing pipeline. Owns the dlib models and orchestrates the
/// full detect → align → landmark → morph → video-write workflow.
class Pipeline {
public:
    /// Construct with path to shape_predictor_68_face_landmarks.dat.
    /// Throws std::runtime_error if the model file cannot be loaded.
    explicit Pipeline(const std::string& landmark_model_path);

    /// Run the full pipeline. Returns true on success.
    /// The optional progress callback is invoked from the calling thread.
    bool run(const PipelineSettings& settings,
             ProgressCallback progress = nullptr);

    /// Last error message (set when run() returns false).
    const std::string& lastError() const { return last_error_; }

private:
    dlib::frontal_face_detector hog_detector_;
    dlib::shape_predictor       shape_predictor_;
    std::string                 last_error_;
};

} // namespace core