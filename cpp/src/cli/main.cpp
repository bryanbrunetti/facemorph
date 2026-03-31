#include "core/pipeline.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static void print_usage(const char* prog) {
    std::cout
        << "Usage: " << prog << " <input_dir> [options]\n"
        << "\n"
        << "Positional arguments:\n"
        << "  input_dir                  Directory containing input face images\n"
        << "\n"
        << "Options:\n"
        << "  -o, --output <path>        Output video path\n"
        << "                             (default: <input_dir>/../output.mp4)\n"
        << "  --fps <float>              Frame rate (default: 24)\n"
        << "  --duration <int>           Hold duration per image in ms\n"
        << "                             (default: 0 = one frame)\n"
        << "  --morph-steps <int>        Number of transition frames between images\n"
        << "                             (default: 0 = hard cut)\n"
        << "  --size <WxH>               Force output frame size, e.g. 512x512\n"
        << "                             (default: auto from first image)\n"
        << "  --save-frames              Save individual aligned/morphed frames\n"
        << "  --model <path>             Path to shape_predictor_68_face_landmarks.dat\n"
        << "                             (default: models/shape_predictor_68_face_landmarks.dat)\n"
        << "  -h, --help                 Show this help message and exit\n";
}

static bool parse_size(const std::string& s, int& width, int& height) {
    // Expected format: WxH  (e.g. "512x512" or "1920x1080")
    auto pos = s.find('x');
    if (pos == std::string::npos) {
        pos = s.find('X');
    }
    if (pos == std::string::npos) {
        return false;
    }
    try {
        width  = std::stoi(s.substr(0, pos));
        height = std::stoi(s.substr(pos + 1));
    } catch (...) {
        return false;
    }
    return width >= 0 && height >= 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    core::PipelineSettings settings;
    std::string model_path = "models/shape_predictor_68_face_landmarks.dat";
    bool input_set = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 >= argc) {
                std::cerr << "Error: " << arg << " requires an argument.\n";
                return 1;
            }
            settings.output_path = fs::path(argv[++i]);
        } else if (arg == "--fps") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --fps requires an argument.\n";
                return 1;
            }
            try {
                settings.fps = std::stod(argv[++i]);
            } catch (...) {
                std::cerr << "Error: invalid value for --fps.\n";
                return 1;
            }
            if (settings.fps <= 0.0) {
                std::cerr << "Error: --fps must be positive.\n";
                return 1;
            }
        } else if (arg == "--duration") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --duration requires an argument.\n";
                return 1;
            }
            try {
                settings.duration_ms = std::stoi(argv[++i]);
            } catch (...) {
                std::cerr << "Error: invalid value for --duration.\n";
                return 1;
            }
            if (settings.duration_ms < 0) {
                std::cerr << "Error: --duration must be non-negative.\n";
                return 1;
            }
        } else if (arg == "--morph-steps") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --morph-steps requires an argument.\n";
                return 1;
            }
            try {
                settings.morph_steps = std::stoi(argv[++i]);
            } catch (...) {
                std::cerr << "Error: invalid value for --morph-steps.\n";
                return 1;
            }
            if (settings.morph_steps < 0) {
                std::cerr << "Error: --morph-steps must be non-negative.\n";
                return 1;
            }
        } else if (arg == "--size") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --size requires an argument (e.g. 512x512).\n";
                return 1;
            }
            if (!parse_size(argv[++i], settings.width, settings.height)) {
                std::cerr << "Error: invalid --size format. Expected WxH (e.g. 512x512).\n";
                return 1;
            }
        } else if (arg == "--save-frames") {
            settings.save_frames = true;
        } else if (arg == "--model") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --model requires an argument.\n";
                return 1;
            }
            model_path = argv[++i];
        } else if (arg[0] == '-') {
            std::cerr << "Error: unknown option '" << arg << "'.\n";
            print_usage(argv[0]);
            return 1;
        } else {
            // Positional argument: input directory
            if (input_set) {
                std::cerr << "Error: unexpected positional argument '" << arg << "'.\n";
                return 1;
            }
            settings.input_dir = fs::path(arg);
            input_set = true;
        }
    }

    // ── Validate required arguments ──────────────────────────────────────
    if (!input_set) {
        std::cerr << "Error: input directory is required.\n";
        print_usage(argv[0]);
        return 1;
    }

    if (!fs::is_directory(settings.input_dir)) {
        std::cerr << "Error: '" << settings.input_dir.string()
                  << "' is not a valid directory.\n";
        return 1;
    }

    settings.model_path = fs::path(model_path);
    if (!fs::exists(settings.model_path)) {
        std::cerr << "Error: landmark model not found at '"
                  << settings.model_path.string() << "'.\n";
        return 1;
    }

    // Default output path
    if (settings.output_path.empty()) {
        settings.output_path = settings.input_dir.parent_path() / "output.mp4";
    }

    // ── Summary ──────────────────────────────────────────────────────────
    std::cout << "Image Transition CLI\n"
              << "  Input dir   : " << settings.input_dir.string() << "\n"
              << "  Output file : " << settings.output_path.string() << "\n"
              << "  Model       : " << settings.model_path.string() << "\n"
              << "  FPS         : " << settings.fps << "\n"
              << "  Hold (ms)   : " << settings.duration_ms << "\n"
              << "  Morph steps : " << settings.morph_steps << "\n"
              << "  Frame size  : "
              << (settings.width == 0 && settings.height == 0
                      ? "auto"
                      : std::to_string(settings.width) + "x" +
                            std::to_string(settings.height))
              << "\n"
              << "  Save frames : " << (settings.save_frames ? "yes" : "no")
              << "\n\n";

    // ── Run pipeline ─────────────────────────────────────────────────────
    try {
        core::Pipeline pipeline(settings.model_path.string());

        bool ok = pipeline.run(settings,
            [](int current, int total, const std::string& msg) {
                if (total > 0) {
                    int pct = current * 100 / total;
                    std::cout << "\r[" << pct << "%] "
                              << current << "/" << total;
                }
                if (!msg.empty()) {
                    std::cout << "  " << msg;
                }
                std::cout << std::flush;
            });

        std::cout << "\n";

        if (ok) {
            std::cout << "Done! Output written to "
                      << settings.output_path.string() << "\n";
            return 0;
        } else {
            std::cerr << "Pipeline failed: " << pipeline.lastError() << "\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "\nFatal error: " << e.what() << "\n";
        return 1;
    }
}