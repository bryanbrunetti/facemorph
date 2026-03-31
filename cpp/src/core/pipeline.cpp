#include "pipeline.h"
#include "face_align.h"
#include "landmarks.h"
#include "delaunay.h"
#include "morph.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>

namespace core {

// ---------------------------------------------------------------------------
// Internal helper struct for holding per-image data during processing
// ---------------------------------------------------------------------------
struct AlignedEntry {
    cv::Mat         image;
    LandmarkResult  landmarks;
    FaceKeypoints   keypoints;
    std::string     filename;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {

/// Case-insensitive check for supported image extensions.
bool isSupportedImage(const std::filesystem::path& p) {
    static const std::set<std::string> exts = {
        ".jpg", ".jpeg", ".png", ".webp", ".bmp", ".tiff"
    };
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return exts.count(ext) != 0;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Pipeline construction
// ---------------------------------------------------------------------------
Pipeline::Pipeline(const std::string& landmark_model_path) {
    if (!std::filesystem::exists(landmark_model_path)) {
        throw std::runtime_error(
            "Landmark model file not found: " + landmark_model_path);
    }

    hog_detector_ = dlib::get_frontal_face_detector();

    try {
        dlib::deserialize(landmark_model_path) >> shape_predictor_;
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("Failed to load landmark model: ") + e.what());
    }
}

// ---------------------------------------------------------------------------
// Pipeline::run
// ---------------------------------------------------------------------------
bool Pipeline::run(const PipelineSettings& settings,
                   ProgressCallback progress) {
    try {
        // ---------------------------------------------------------------
        // 0. Collect and sort image files
        // ---------------------------------------------------------------
        std::vector<std::filesystem::path> files;
        for (const auto& entry : std::filesystem::directory_iterator(settings.input_dir)) {
            if (entry.is_regular_file() && isSupportedImage(entry.path())) {
                files.push_back(entry.path());
            }
        }
        std::sort(files.begin(), files.end());

        if (files.empty()) {
            last_error_ = "No supported image files found in " +
                          settings.input_dir.string();
            std::cout << last_error_ << "\n";
            return false;
        }
        std::cout << "Found " << files.size() << " image(s)\n";

        // Hold-frame count
        const int hold_count =
            (settings.duration_ms > 0)
                ? std::max(1, static_cast<int>(
                      std::round(settings.fps * settings.duration_ms / 1000.0)))
                : 1;

        // Optional frames directory
        std::filesystem::path frames_dir;
        if (settings.save_frames) {
            frames_dir = settings.output_path.parent_path() / "frames";
            std::filesystem::create_directories(frames_dir);
        }

        // ---------------------------------------------------------------
        // Phase 1 – Align every image
        // ---------------------------------------------------------------
        std::vector<AlignedEntry> aligned;
        aligned.reserve(files.size());

        cv::Size frame_size(settings.width, settings.height);

        const int total_phase1 = static_cast<int>(files.size());
        for (int idx = 0; idx < total_phase1; ++idx) {
            const auto& file = files[idx];
            std::cout << "Aligning [" << (idx + 1) << "/" << total_phase1
                      << "] " << file.filename().string() << "\n";
            if (progress) {
                progress(idx + 1, total_phase1,
                         "Aligning " + file.filename().string());
            }

            cv::Mat bgr = cv::imread(file.string());
            if (bgr.empty()) {
                std::cout << "  WARNING: could not read " << file.string()
                          << ", skipping\n";
                continue;
            }

            auto result = alignFace(bgr, file.filename().string(),
                                    hog_detector_, shape_predictor_);
            if (!result.has_value()) {
                std::cout << "  WARNING: no face detected in "
                          << file.filename().string() << ", skipping\n";
                continue;
            }

            cv::Mat frame = std::move(result->image);
            FaceKeypoints kps = result->keypoints;

            // Determine / enforce frame size
            if (frame_size.width == 0 || frame_size.height == 0) {
                frame_size = cv::Size(frame.cols, frame.rows);
            }
            if (frame.cols != frame_size.width ||
                frame.rows != frame_size.height) {
                cv::resize(frame, frame, frame_size, 0, 0, cv::INTER_CUBIC);
            }

            // Optionally extract landmarks for morphing
            AlignedEntry entry;
            entry.image    = frame;
            entry.keypoints = kps;
            entry.filename = file.filename().string();

            if (settings.morph_steps > 0) {
                entry.landmarks = getLandmarks(frame, kps,
                                              hog_detector_, shape_predictor_);
            }

            aligned.push_back(std::move(entry));

            // Save aligned frame if requested
            if (settings.save_frames) {
                std::string out_name = "c_" + file.filename().string();
                cv::imwrite((frames_dir / out_name).string(), aligned.back().image);
            }
        }

        if (aligned.empty()) {
            last_error_ = "No faces could be aligned from the input images.";
            std::cout << last_error_ << "\n";
            return false;
        }
        std::cout << "Aligned " << aligned.size() << " face(s)\n";

        // ---------------------------------------------------------------
        // Phase 2 – Write video
        // ---------------------------------------------------------------
        const int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
        cv::VideoWriter writer(settings.output_path.string(), fourcc,
                               settings.fps, frame_size);
        if (!writer.isOpened()) {
            last_error_ = "Failed to open VideoWriter for " +
                          settings.output_path.string();
            std::cout << last_error_ << "\n";
            return false;
        }

        const int n_aligned = static_cast<int>(aligned.size());
        // Total steps for progress: each image writes hold frames + morph frames
        const int total_phase2 = n_aligned;

        for (int i = 0; i < n_aligned; ++i) {
            const auto& entry = aligned[i];
            std::cout << "Writing [" << (i + 1) << "/" << n_aligned << "] "
                      << entry.filename << "\n";
            if (progress) {
                progress(i + 1, total_phase2,
                         "Writing " + entry.filename);
            }

            // Write hold frames
            for (int h = 0; h < hold_count; ++h) {
                writer.write(entry.image);
            }

            // Morph transition to next image
            if (settings.morph_steps > 0 && i < n_aligned - 1) {
                const auto& cur  = aligned[i];
                const auto& next = aligned[i + 1];

                std::vector<cv::Point2f> pts_cur  = cur.landmarks.points;
                std::vector<cv::Point2f> pts_next = next.landmarks.points;

                const int h = frame_size.height;
                const int w = frame_size.width;

                // Handle mismatched landmark counts: downgrade both to
                // 5 keypoints + 8 boundary points.
                if (pts_cur.size() != pts_next.size()) {
                    std::cout << "  Landmark count mismatch ("
                              << pts_cur.size() << " vs "
                              << pts_next.size()
                              << "), using 5-point + boundary fallback\n";

                    auto kpVec_cur  = cur.keypoints.toVector();
                    auto kpVec_next = next.keypoints.toVector();

                    std::vector<cv::Point2f> boundary = {
                        {1.0f, 1.0f},
                        {w / 2.0f, 1.0f},
                        {w - 2.0f, 1.0f},
                        {1.0f, h / 2.0f},
                        {w - 2.0f, h / 2.0f},
                        {1.0f, h - 2.0f},
                        {w / 2.0f, h - 2.0f},
                        {w - 2.0f, h - 2.0f}
                    };

                    pts_cur = kpVec_cur;
                    pts_cur.insert(pts_cur.end(), boundary.begin(), boundary.end());

                    pts_next = kpVec_next;
                    pts_next.insert(pts_next.end(), boundary.begin(), boundary.end());
                }

                // Compute midpoints for Delaunay
                std::vector<cv::Point2f> mid_pts(pts_cur.size());
                for (size_t p = 0; p < pts_cur.size(); ++p) {
                    mid_pts[p].x = (pts_cur[p].x + pts_next[p].x) * 0.5f;
                    mid_pts[p].y = (pts_cur[p].y + pts_next[p].y) * 0.5f;
                }

                auto triangles = computeDelaunay(mid_pts, frame_size);
                std::cout << "  Morphing " << cur.filename << " -> "
                          << next.filename << " (" << settings.morph_steps
                          << " steps, " << triangles.size()
                          << " triangles)\n";

                for (int step = 1; step <= settings.morph_steps; ++step) {
                    float t = static_cast<float>(step) /
                              static_cast<float>(settings.morph_steps + 1);
                    cv::Mat morphed = morphPair(cur.image, next.image,
                                               pts_cur, pts_next,
                                               triangles, t);
                    writer.write(morphed);

                    if (settings.save_frames) {
                        std::string morph_name =
                            "morph_" + std::to_string(i) + "_" +
                            std::to_string(i + 1) + "_" +
                            std::to_string(step) + ".png";
                        cv::imwrite((frames_dir / morph_name).string(), morphed);
                    }
                }
            }
        }

        writer.release();
        std::cout << "Video saved to " << settings.output_path.string() << "\n";
        return true;

    } catch (const std::exception& e) {
        last_error_ = std::string("Pipeline error: ") + e.what();
        std::cout << last_error_ << "\n";
        return false;
    } catch (...) {
        last_error_ = "Pipeline encountered an unknown error.";
        std::cout << last_error_ << "\n";
        return false;
    }
}

} // namespace core