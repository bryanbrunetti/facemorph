# Image Transition — C++ / wxWidgets

## Overview

A C++ rewrite of the original Python face-morphing video tool. **Image Transition** detects and aligns faces in a directory of photos, then assembles them into a video with smooth Delaunay-based morph transitions.

Under the hood it uses:

- **dlib** for face detection and 68-point landmark extraction
- **OpenCV** for image processing, triangulation, and video encoding
- **wxWidgets** for a native cross-platform GUI (Windows `.exe`, macOS `.app`)

The result is a single, self-contained application that runs at native speed with no Python runtime required.

> For full build instructions, usage examples, and troubleshooting, see the
> [root README](../README.md).

---

## Features

| Feature | Details |
|---|---|
| **Face detection & alignment** | Automatic face centering and rotation correction via dlib's HOG frontal-face detector and 68-point shape predictor |
| **68-point landmark extraction** | Full dlib landmark model with an automatic 5-point fallback when dlib cannot locate a face in the aligned image |
| **Delaunay triangulation morphing** | Per-triangle affine warping produces smooth, artifact-free transitions between consecutive face images |
| **Native GUI** | wxWidgets interface using real platform controls — Win32 on Windows, Cocoa on macOS — not drawn imitations |
| **CLI mode** | Scriptable command-line interface for batch processing and CI/CD pipelines |
| **Configurable output** | Control FPS, hold duration per photo, morph step count, and output resolution |
| **Frame export** | Optionally save every aligned and morphed frame as a PNG for external editing |

---

## Dependencies

| Dependency | Minimum Version | Notes |
|---|---|---|
| **CMake** | 3.20+ | Build-system generator |
| **C++ compiler** | C++17 support | GCC 9+, Clang 10+, or MSVC 2019+ |
| **OpenCV** | 4.5+ | Modules: `core`, `imgproc`, `imgcodecs`, `videoio` |
| **wxWidgets** | 3.2+ | Components: `core`, `base` |
| **dlib** | 19.24+ | **Automatically fetched** by CMake `FetchContent` — no manual install required |

---

## Model Files

The dlib shape predictor model (`shape_predictor_68_face_landmarks.dat`) is **not** bundled with this repository (it is ~100 MB). You must download it separately.

See [`models/README.md`](models/README.md) for detailed download instructions.

**Quick setup:**

```sh
cd models
curl -LO http://dlib.net/files/shape_predictor_68_face_landmarks.dat.bz2
bunzip2 shape_predictor_68_face_landmarks.dat.bz2
```

Alternatively, pass `--model /path/to/model.dat` to the CLI or set the model path in the GUI file picker.

---

## Building

### macOS

Install dependencies with [Homebrew](https://brew.sh):

```sh
brew install cmake opencv wxwidgets
```

Then build:

```sh
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

The GUI target produces a native `.app` bundle at `build/image_transition_gui.app`.

```sh
open build/image_transition_gui.app
```

### Windows

Install dependencies with [vcpkg](https://vcpkg.io):

```powershell
vcpkg install opencv4 wxwidgets
```

Then build, pointing CMake at the vcpkg toolchain file:

```powershell
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build . --config Release
```

The GUI target produces `build\Release\image_transition_gui.exe`.

> **Tip:** If you use Visual Studio, open the project folder directly — VS will
> detect the `CMakeLists.txt` and configure automatically. Set the vcpkg
> toolchain file in your CMake settings.

### Linux

Install dependencies with your system package manager.

**Ubuntu / Debian:**

```sh
sudo apt update
sudo apt install cmake g++ libopencv-dev libwxgtk3.2-dev
```

**Fedora:**

```sh
sudo dnf install cmake gcc-c++ opencv-devel wxGTK-devel
```

**Arch Linux:**

```sh
sudo pacman -S cmake opencv wxwidgets-gtk3
```

Then build:

```sh
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### Build Notes

- dlib is fetched and compiled automatically on the first build via CMake
  `FetchContent`. This may take several minutes. Subsequent builds are
  incremental and fast.
- To speed up the initial build, use parallel compilation:

  ```sh
  cmake --build . --config Release --parallel $(nproc)
  ```

- The build produces two executables:

  | Target | Description |
  |---|---|
  | `image_transition_cli` | Command-line tool |
  | `image_transition_gui` | Native graphical application |

---

## Usage

### CLI

```
image_transition_cli <input_dir> [options]
```

| Flag | Default | Description |
|---|---|---|
| `-o`, `--output <path>` | `<input_dir>/../output.mp4` | Output video file path |
| `--fps <float>` | `24` | Frames per second in the output video |
| `--duration <int>` | `0` | Milliseconds each image is held (`0` = exactly one frame) |
| `--morph-steps <int>` | `0` | Intermediate morph frames between each pair (`0` = hard cut) |
| `--size <WxH>` | *auto* | Force output size, e.g. `1280x720` (default: first aligned image dimensions) |
| `--save-frames` | *off* | Save aligned and morphed frames as PNGs alongside the video |
| `--model <path>` | `models/shape_predictor_68_face_landmarks.dat` | Path to the dlib 68-point shape predictor model |
| `-h`, `--help` | | Show usage information and exit |

**Examples:**

```sh
# Simple slideshow — no morphing, 2 seconds per photo
./image_transition_cli ./photos --fps 1 --duration 2000 -o slideshow.mp4

# Smooth morph transitions at 30 fps
./image_transition_cli ./photos \
    --output morph_video.mp4 \
    --fps 30 \
    --duration 1000 \
    --morph-steps 45

# Fixed resolution with frame export
./image_transition_cli ./photos \
    --output output.mp4 \
    --fps 24 \
    --morph-steps 30 \
    --size 512x512 \
    --save-frames
```

### GUI

Launch the graphical interface:

```sh
# macOS
open build/image_transition_gui.app

# Windows
build\Release\image_transition_gui.exe

# Linux
./build/image_transition_gui
```

The GUI provides:

1. **Input Folder** — select the directory containing your face images
2. **Output File** — choose where to save the resulting `.mp4` video
3. **Model File** — point to `shape_predictor_68_face_landmarks.dat`
4. **Video Settings** — adjust FPS, hold duration, morph steps, frame size
5. **Generate Video** — runs the pipeline in a background thread with a live
   progress bar and log output

---

## Project Structure

```
cpp/
├── CMakeLists.txt                  # Top-level CMake build configuration
├── README.md                       # This file
│
├── models/
│   └── README.md                   # Download instructions for dlib models
│
├── packaging/
│   ├── macos/
│   │   └── Info.plist              # macOS .app bundle metadata
│   └── windows/                    # (reserved for installer scripts)
│
└── src/
    ├── core/                       # Platform-independent processing library
    │   ├── types.h                 # Shared data types and settings struct
    │   ├── face_align.h / .cpp     # Face detection, rotation, alignment
    │   ├── landmarks.h / .cpp      # 68-point (+ 5-point fallback) extraction
    │   ├── delaunay.h / .cpp       # Delaunay triangulation via cv::Subdiv2D
    │   ├── morph.h / .cpp          # Per-triangle affine warp and blending
    │   └── pipeline.h / .cpp       # Orchestration: images → aligned → video
    │
    ├── cli/
    │   └── main.cpp                # Command-line entry point
    │
    └── gui/
        ├── App.h / .cpp            # wxApp subclass (application lifecycle)
        └── MainFrame.h / .cpp      # Main window: controls, progress, log
```

### Architecture

The project is split into three layers:

- **`core/`** — a static library (`image_transition_core`) containing all face
  processing, morphing, and video-writing logic. It has no GUI dependencies and
  communicates progress through a `std::function` callback.

- **`gui/`** — a wxWidgets application linked against the core library.
  Pipeline work runs on a background `std::thread`; progress updates are
  marshalled back to the UI thread via `wxCallAfter`.

- **`cli/`** — a command-line executable also linked against the core library.
  It parses `argv` directly (no external library) and prints progress to
  stdout.

---

## License

See the repository root for license information.

The dlib shape-predictor models are distributed under the
[Boost Software License](https://www.boost.org/LICENSE_1_0.txt).
OpenCV is distributed under the
[Apache 2.0 License](https://opencv.org/license/).
wxWidgets is distributed under the
[wxWindows Library Licence](https://www.wxwidgets.org/about/licence/).