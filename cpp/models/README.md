# Model Files

This directory holds the **dlib shape-predictor models** used for facial-landmark
detection. The models are too large to commit to version control and must be
downloaded separately.

---

## Required Model

| File | Size (approx.) | Purpose |
|---|---|---|
| `shape_predictor_68_face_landmarks.dat` | ~100 MB | 68-point facial-landmark detection (primary) |

### Download & Install

1. **Download** the compressed model from dlib's official repository:

   <http://dlib.net/files/shape_predictor_68_face_landmarks.dat.bz2>

2. **Decompress** the archive:

   ```sh
   # macOS / Linux
   bunzip2 shape_predictor_68_face_landmarks.dat.bz2

   # Windows (PowerShell, using 7-Zip)
   7z x shape_predictor_68_face_landmarks.dat.bz2
   ```

3. **Place** the resulting `.dat` file in this directory so the final path is:

   ```
   cpp/models/shape_predictor_68_face_landmarks.dat
   ```

### Verification

After decompression you should have a single file of roughly 99.7 MB:

```sh
ls -lh shape_predictor_68_face_landmarks.dat
# -rw-r--r--  1 user  staff    99M  ...  shape_predictor_68_face_landmarks.dat
```

---

## Alternative: Custom Model Path

If you prefer to keep the model file somewhere else on disk, you can point the
application to it instead of placing it in this directory:

- **CLI** — pass the `--model` flag:

  ```sh
  ./image_transition_cli <input_dir> --model /path/to/shape_predictor_68_face_landmarks.dat
  ```

- **GUI** — set the model path in **Preferences → Model File** (or use the
  file-picker dialog on first launch).

---

## Optional: 5-Point Model (Lightweight Fallback)

For faster (but less accurate) alignment you can also download the smaller
5-point predictor:

| File | Size (approx.) | Purpose |
|---|---|---|
| `shape_predictor_5_face_landmarks.dat` | ~9 MB | 5-point landmark detection (fallback) |

Download URL: <http://dlib.net/files/shape_predictor_5_face_landmarks.dat.bz2>

The application will automatically fall back to the 5-point model if it is
present in this directory and the 68-point model cannot be found.

---

## License

The dlib shape-predictor models were trained by Davis King and are distributed
under the **Boost Software License**. See the
[dlib model page](http://dlib.net/face_landmark_detection.py.html) for details.