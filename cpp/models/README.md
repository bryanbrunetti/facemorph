# Model Files

This directory holds the **dlib shape-predictor model** used for facial-landmark
detection.

---

## Automatic Download

The model is **downloaded automatically** by CMake during the configure step:

```sh
cd cpp
mkdir build && cd build
cmake ..   # ← downloads and decompresses the model here
```

CMake fetches `shape_predictor_68_face_landmarks.dat.bz2` (~100 MB) from
`http://dlib.net/files/`, decompresses it, and places the resulting
`shape_predictor_68_face_landmarks.dat` in this directory.  The compressed
archive is deleted afterwards.

The model is also copied into `build/models/` so both the CLI and GUI
executables find it without requiring any `--model` flag.  On macOS the model
is additionally embedded inside the `.app` bundle's
`Contents/Resources/models/` folder for a fully self-contained application.

---

## Required Model

| File | Size (approx.) | Purpose |
|---|---|---|
| `shape_predictor_68_face_landmarks.dat` | ~100 MB | 68-point facial-landmark detection (primary) |

---

## Alternative: Custom Model Path

If you prefer to use a model stored elsewhere on disk:

- **CLI** — pass the `--model` flag:

  ```sh
  ./image_transition_cli <input_dir> --model /path/to/shape_predictor_68_face_landmarks.dat
  ```

- **GUI** — use the **Model File** picker to select an alternative file.

---

## License

The dlib shape-predictor models were trained by Davis King and are distributed
under the **Boost Software License**. See the
[dlib model page](http://dlib.net/face_landmark_detection.py.html) for details.