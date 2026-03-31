# Image Transition

Detect and align faces in a directory of photos, then assemble them into a video
with smooth Delaunay-based morph transitions.

Built with **C++17**, using **dlib** for face detection and 68-point landmark
extraction, **OpenCV** for image processing and video encoding, and **wxWidgets**
for a native cross-platform GUI.

The result is a single, self-contained native application — Windows `.exe` or
macOS `.app` — with no Python runtime or interpreter required.

---

## Table of Contents

- [Features](#features)
- [Screenshots](#screenshots)
- [Dependencies](#dependencies)
- [Model Files](#model-files)
- [Building](#building)
  - [macOS](#macos)
  - [Windows](#windows)
  - [Linux](#linux)
- [Usage — CLI](#usage--cli)
- [Usage — GUI](#usage--gui)
- [Project Structure](#project-structure)
- [How It Works](#how-it-works)
- [Troubleshooting](#troubleshooting)
- [License](#license)

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

## Screenshots

*Coming soon.*

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

The dlib 68-point shape predictor model (~100 MB) is **automatically downloaded
and decompressed** by CMake during the configure step. No manual setup is
required — simply run `cmake ..` and the model will appear in `cpp/models/`.

The model is also:

- **Copied into the build directory** (`build/models/`) so the CLI and GUI
  executables find it automatically without needing `--model`.
- **Embedded inside the macOS `.app` bundle** under `Contents/Resources/models/`
  so the GUI is fully self-contained.

If you prefer to use a model file from a different location, you can still
override the path:

- **CLI** — pass `--model <path>`:

  ```sh
  ./image_transition_cli ./photos --model /path/to/shape_predictor_68_face_landmarks.dat
  ```

- **GUI** — use the **Model File** picker to select an alternative file.

See [`cpp/models/README.md`](cpp/models/README.md) for more details.

---

## Building

All platforms follow the same general pattern:

```sh
cd cpp
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

This produces two executables:

| Target | Description |
|---|---|
| `image_transition_cli` | Command-line tool |
| `image_transition_gui` | Native graphical application (`.exe` on Windows, `.app` on macOS) |

> **Note:** dlib is fetched and compiled automatically by CMake on the first
> build. This may take several minutes the first time. Subsequent builds will
> be fast.

### macOS

Install the required libraries with [Homebrew](https://brew.sh):

```sh
brew install cmake opencv wxwidgets
```

Then build:

```sh
cd cpp
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

The GUI target produces a native `.app` bundle:

```
build/image_transition_gui.app
```

You can launch it from the terminal:

```sh
open build/image_transition_gui.app
```

Or copy it to `/Applications` for permanent installation.

### Windows

Install the required libraries with [vcpkg](https://vcpkg.io):

```powershell
vcpkg install opencv4 wxwidgets
```

Then build, pointing CMake at the vcpkg toolchain file:

```powershell
cd cpp
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build . --config Release
```

The GUI target produces:

```
build\Release\image_transition_gui.exe
```

> **Tip:** If you use Visual Studio, you can also open the `cpp` folder
> directly — VS will detect the `CMakeLists.txt` and configure automatically.
> Set the vcpkg toolchain file in your CMake settings.

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

Then follow the general build steps:

```sh
cd cpp
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

---

## Usage — CLI

```
image_transition_cli <input_dir> [options]
```

### Options

| Flag | Default | Description |
|---|---|---|
| `-o`, `--output <path>` | `<input_dir>/../output.mp4` | Output video file path |
| `--fps <float>` | `24` | Frames per second in the output video |
| `--duration <int>` | `0` | Milliseconds each image is held before the next transition (`0` = exactly one frame) |
| `--morph-steps <int>` | `0` | Number of intermediate morph frames between each pair of images (`0` = hard cut) |
| `--size <WxH>` | *auto* | Force output frame size, e.g. `1280x720` (default: dimensions of the first aligned image) |
| `--save-frames` | *off* | Save individual aligned and morphed frames as PNGs alongside the video |
| `--model <path>` | *auto* (next to executable) | Path to the dlib 68-point shape predictor model |
| `-h`, `--help` | | Show usage information and exit |

### Examples

**Simple slideshow (no morphing):**

```sh
./image_transition_cli ./photos --fps 1 --duration 2000 -o slideshow.mp4
```

**Smooth morph transitions at 30 fps:**

```sh
./image_transition_cli ./photos \
    --output morph_video.mp4 \
    --fps 30 \
    --duration 1000 \
    --morph-steps 45 \
    --model /path/to/shape_predictor_68_face_landmarks.dat
```

**Fixed resolution with frame export:**

```sh
./image_transition_cli ./photos \
    --output output.mp4 \
    --fps 24 \
    --morph-steps 30 \
    --size 512x512 \
    --save-frames
```

---

## Usage — GUI

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

All controls use native platform widgets (Win32 on Windows, Cocoa on macOS, GTK
on Linux) so the application looks and feels like it belongs on your OS.

---

## Project Structure

```
image-transition/
├── README.md                           ← this file
├── main.py                             ← original Python implementation
│
└── cpp/                                ← C++ / wxWidgets rewrite
    ├── CMakeLists.txt                  # Top-level CMake build configuration
    ├── README.md                       # C++-specific notes
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
  communicates progress through a simple callback function.

- **`gui/`** — a wxWidgets application that links against the core library.
  Pipeline work runs on a background `std::thread`; progress updates are
  marshalled back to the UI thread via `wxCallAfter`.

- **`cli/`** — a command-line executable that also links against the core
  library. It parses `argv` directly (no external arg-parsing library) and
  prints progress to stdout.

---

## How It Works

The processing pipeline mirrors the original Python implementation:

1. **File discovery** — scan the input directory for images (`.jpg`, `.jpeg`,
   `.png`, `.webp`, `.bmp`, `.tiff`), sorted alphabetically.

2. **Face alignment** — for each image, detect the primary face using dlib's
   HOG frontal-face detector, extract 68 landmarks with the shape predictor,
   compute left/right eye centers from the landmark points, then apply an
   affine transformation that rotates and scales the image so the eyes are
   horizontal and centered.

3. **Landmark extraction** — run dlib again on the aligned image to get
   high-quality 68-point landmarks in the aligned coordinate space. If dlib
   fails to detect a face in the aligned image, fall back to the 5 keypoints
   (eye centers, nose tip, mouth corners) derived during alignment. Eight
   boundary points (image corners + edge midpoints) are always appended.

4. **Delaunay triangulation** — compute a triangulation over the midpoint
   landmarks of each consecutive image pair using OpenCV's `cv::Subdiv2D`.

5. **Morphing** — for each transition step, interpolate the landmark positions,
   warp both source images into the intermediate landmark configuration using
   per-triangle affine transforms, then alpha-blend the two warped images.

6. **Video writing** — hold each aligned image for the configured number of
   frames, insert the morph transition frames between consecutive images, and
   encode everything to MP4 using OpenCV's `cv::VideoWriter`.

---

## Troubleshooting

### `No face detected` for every image

- Make sure the images contain clearly visible, front-facing human faces.
- dlib's HOG detector works best on upright faces with good lighting. Very
  small faces, extreme angles, or heavy occlusion may fail detection.

### CMake cannot find OpenCV or wxWidgets

- **macOS:** make sure you ran `brew install opencv wxwidgets` and that
  Homebrew's prefix is on your `CMAKE_PREFIX_PATH`.
- **Windows:** make sure you pass `-DCMAKE_TOOLCHAIN_FILE=...` pointing to
  your vcpkg toolchain file.
- **Linux:** install the `-dev` packages for your distribution (e.g.
  `libopencv-dev`, `libwxgtk3.2-dev`).

### dlib build takes a long time

This is expected on the first build — dlib is compiled from source via
`FetchContent`. Subsequent builds will be incremental and fast. To speed up
the initial build:

```sh
cmake --build . --config Release --parallel $(nproc)
```

### Video is empty or zero-length

- Check that `--morph-steps` is set to a value greater than 0 if you want
  morph transitions (the default is 0, which produces hard cuts).
- Check that `--duration` is set if you want each image held for more than
  one frame (the default is 0, meaning exactly one frame per image).

### Model file not found

- Re-run `cmake ..` from the `cpp/build` directory — it will download the model
  automatically if it is missing from `cpp/models/`.
- If the download fails (e.g., no internet access), place
  `shape_predictor_68_face_landmarks.dat` in `cpp/models/` manually, then
  re-run `cmake ..` (it will skip the download and copy the file).
- Pass the correct path via `--model` or the GUI file picker if you store the
  model in a custom location.

---

## License

See the repository root for license information.

The dlib shape-predictor models are distributed under the
[Boost Software License](https://www.boost.org/LICENSE_1_0.txt).
OpenCV is distributed under the
[Apache 2.0 License](https://opencv.org/license/).
wxWidgets is distributed under the
[wxWindows Library Licence](https://www.wxwidgets.org/about/licence/).