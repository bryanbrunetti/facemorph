import argparse
from pathlib import Path

import cv2
import dlib
import face_recognition
import face_recognition_models
import numpy as np

# ── Global initialisation ────────────────────────────────────────────────────
# Prefer GPU providers for ONNX Runtime (used by MTCNN)
import onnxruntime as ort
from mtcnn_ort import MTCNN


def _select_ort_providers():
    """Pick the best ONNX Runtime providers, testing CUDA actually works."""
    available = ort.get_available_providers()
    if "CUDAExecutionProvider" in available:
        try:
            # Tiny smoke-test: create a trivial session with CUDA to see if
            # the shared libs (libcublasLt, cuDNN, etc.) can actually load.
            import os
            import tempfile

            # Minimal valid ONNX model (identity graph, 1 float input/output)
            _TINY_ONNX = (
                b"\x08\x07\x12\x04onnx\x1a\x011:\xab\x01\n\x1c\n\x01x\x12\x01y"
                b'\x1a\x08Identity"\x08Identity\x12\x10test_ort_cuda_ok'
                b"\x1a\x011*\x011Z\x1f\n\x01x\x12\x1a\n\x18\x08\x01\x12\x14\n"
                b"\x02\x08\x01\n\x02\x08\x01\n\x02\x08\x01\n\x02\x08\x01\n\x02"
                b"\x08\x01b\x1f\n\x01y\x12\x1a\n\x18\x08\x01\x12\x14\n\x02\x08"
                b"\x01\n\x02\x08\x01\n\x02\x08\x01\n\x02\x08\x01\n\x02\x08\x01"
            )
            tmp = tempfile.NamedTemporaryFile(suffix=".onnx", delete=False)
            tmp.write(_TINY_ONNX)
            tmp.close()
            try:
                sess = ort.InferenceSession(
                    tmp.name,
                    providers=["CUDAExecutionProvider", "CPUExecutionProvider"],
                )
                if "CUDAExecutionProvider" in sess.get_providers():
                    print("ONNX Runtime: using CUDAExecutionProvider (GPU)")
                    return ["CUDAExecutionProvider", "CPUExecutionProvider"]
            finally:
                os.unlink(tmp.name)
        except Exception:
            pass
    print("ONNX Runtime: using CPUExecutionProvider")
    return ["CPUExecutionProvider"]


_ort_providers = _select_ort_providers()


class _OrtSession:
    """ONNX Runtime session wrapper that honours our provider preference."""

    def __init__(self, path):
        self.session = ort.InferenceSession(path, providers=_ort_providers)
        assert len(self.session.get_inputs()) == 1
        self.input_name = self.session.get_inputs()[0].name

    def __call__(self, input_array):
        import numpy as _np

        if input_array.dtype == _np.float64:
            input_array = input_array.astype(_np.float32)
        return self.session.run(None, {self.input_name: input_array})


detector = MTCNN(runner_cls=_OrtSession)

_landmark_predictor = dlib.shape_predictor(
    face_recognition_models.pose_predictor_model_location()
)

# Use dlib's CNN face detector (GPU-accelerated) instead of the HOG detector
_cnn_model_path = face_recognition_models.cnn_face_detector_model_location()
_face_detector = dlib.cnn_face_detection_model_v1(_cnn_model_path)


# ── Face alignment (existing logic) ─────────────────────────────────────────


def align_face(file):
    """
    Detect and align the primary face in an image.
    Returns (aligned_bgr_frame, mtcnn_keypoints_in_aligned_space) or None.
    The keypoints are the 5 MTCNN points (left_eye, right_eye, nose,
    mouth_left, mouth_right) transformed through the same affine matrix
    used to align the image.
    """
    image = face_recognition.load_image_file(file)
    face_locations = detector.detect_faces(image)
    if len(face_locations) == 0:
        print(f"  No face detected in {file.name}, skipping.")
        return None

    kp = face_locations[0]["keypoints"]
    leftEyeCenter = kp["left_eye"]
    rightEyeCenter = kp["right_eye"]

    dY = rightEyeCenter[1] - leftEyeCenter[1]
    dX = rightEyeCenter[0] - leftEyeCenter[0]
    angle = np.degrees(np.arctan2(dY, dX))

    desiredLeftEye = (0.35, 0.35)
    desiredFaceWidth = image.shape[1]
    desiredFaceHeight = image.shape[0]
    desiredRightEyeX = 1.0 - desiredLeftEye[0]

    dist = np.sqrt((dX**2) + (dY**2))
    desiredDist = (desiredRightEyeX - desiredLeftEye[0]) * 300
    scale = desiredDist / dist

    eyesCenter = (
        (leftEyeCenter[0] + rightEyeCenter[0]) // 2,
        (leftEyeCenter[1] + rightEyeCenter[1]) // 2,
    )

    M = cv2.getRotationMatrix2D(eyesCenter, angle, scale)
    M[0, 2] += desiredFaceWidth * 0.5 - eyesCenter[0]
    M[1, 2] += desiredFaceHeight * desiredLeftEye[1] - eyesCenter[1]

    output = cv2.warpAffine(
        image,
        M,
        (desiredFaceWidth, desiredFaceHeight),
        borderMode=cv2.BORDER_CONSTANT,
        flags=cv2.INTER_CUBIC,
    )

    # Transform the 5 MTCNN keypoints through the same affine matrix
    raw_pts = np.array(
        [
            kp["left_eye"],
            kp["right_eye"],
            kp["nose"],
            kp["mouth_left"],
            kp["mouth_right"],
        ],
        dtype=np.float64,
    )
    ones = np.ones((raw_pts.shape[0], 1), dtype=np.float64)
    raw_pts_h = np.hstack([raw_pts, ones])  # Nx3
    transformed = (M @ raw_pts_h.T).T  # Nx2
    mtcnn_kps = [(int(round(p[0])), int(round(p[1]))) for p in transformed]

    return cv2.cvtColor(output, cv2.COLOR_RGB2BGR), mtcnn_kps


# ── Landmark extraction ─────────────────────────────────────────────────────


def get_landmarks(image_bgr, mtcnn_kps):
    """
    Extract facial landmarks + 8 boundary points from a BGR image.

    Tries dlib's 68-point predictor first (best morph quality).  If dlib
    cannot find a face, falls back to the 5 MTCNN keypoints that were
    already successfully detected during alignment — so we always get
    usable landmarks and never need to cross-dissolve.

    Returns a list of (x, y) tuples (76 with dlib, 13 with MTCNN fallback).
    """
    h, w = image_bgr.shape[:2]

    # Boundary points (slightly inset to keep Subdiv2D happy)
    boundary = [
        (1, 1),
        (w // 2, 1),
        (w - 2, 1),
        (1, h // 2),
        (w - 2, h // 2),
        (1, h - 2),
        (w // 2, h - 2),
        (w - 2, h - 2),
    ]

    # Try dlib 68-point landmarks first
    image_rgb = cv2.cvtColor(image_bgr, cv2.COLOR_BGR2RGB)
    faces = _face_detector(image_rgb, 1)
    if faces:
        # CNN detector returns mmod_rectangle objects; extract the .rect
        face_rect = faces[0].rect if hasattr(faces[0], "rect") else faces[0]
        shape = _landmark_predictor(image_rgb, face_rect)
        points = [(shape.part(i).x, shape.part(i).y) for i in range(68)]
        return points + boundary, True

    # Fallback: use the 5 MTCNN keypoints (always available)
    print("    dlib missed face, falling back to MTCNN keypoints")
    return list(mtcnn_kps) + boundary, False


# ── Delaunay helpers ─────────────────────────────────────────────────────────


def compute_delaunay(points, size):
    """
    Compute a Delaunay triangulation over *points* and return a list of
    (i, j, k) index triples.
    """
    w, h = size
    subdiv = cv2.Subdiv2D((0, 0, w, h))

    for p in points:
        x = min(max(1, int(round(p[0]))), w - 2)
        y = min(max(1, int(round(p[1]))), h - 2)
        subdiv.insert((float(x), float(y)))

    triangle_list = subdiv.getTriangleList()
    pts = np.array(points, dtype=np.float32)

    triangles = []
    for t in triangle_list:
        verts = np.array([[t[0], t[1]], [t[2], t[3]], [t[4], t[5]]])
        if (verts[:, 0] < 0).any() or (verts[:, 0] >= w).any():
            continue
        if (verts[:, 1] < 0).any() or (verts[:, 1] >= h).any():
            continue

        indices = []
        for v in verts:
            idx = int(np.argmin(np.sum((pts - v) ** 2, axis=1)))
            indices.append(idx)

        if len(set(indices)) == 3:
            triangles.append(tuple(indices))

    return triangles


def warp_triangle(src, src_tri, dst_tri, dst):
    """Warp a single triangle from *src* into *dst* in-place."""
    r1 = cv2.boundingRect(np.float32([src_tri]))
    r2 = cv2.boundingRect(np.float32([dst_tri]))

    if r1[2] <= 0 or r1[3] <= 0 or r2[2] <= 0 or r2[3] <= 0:
        return

    t1 = [(p[0] - r1[0], p[1] - r1[1]) for p in src_tri]
    t2 = [(p[0] - r2[0], p[1] - r2[1]) for p in dst_tri]

    mask = np.zeros((r2[3], r2[2], 3), dtype=np.float32)
    cv2.fillConvexPoly(mask, np.int32(t2), (1, 1, 1), cv2.LINE_AA)

    # Clamp source crop to image bounds
    y1 = max(0, r1[1])
    x1 = max(0, r1[0])
    y2_c = min(src.shape[0], r1[1] + r1[3])
    x2_c = min(src.shape[1], r1[0] + r1[2])
    if y2_c <= y1 or x2_c <= x1:
        return

    # Contiguous copy avoids stride issues in OpenCV's C++ backend
    crop = np.ascontiguousarray(src[y1:y2_c, x1:x2_c])
    if crop.shape[0] != r1[3] or crop.shape[1] != r1[2]:
        crop = cv2.resize(crop, (r1[2], r1[3]))

    mat = cv2.getAffineTransform(np.float32(t1), np.float32(t2))
    warped = cv2.warpAffine(
        crop,
        mat,
        (r2[2], r2[3]),
        flags=cv2.INTER_LINEAR,
        borderMode=cv2.BORDER_CONSTANT,
    )

    # Clamp destination rect to image bounds
    dy = max(0, r2[1])
    dy2 = min(r2[1] + r2[3], dst.shape[0])
    dx = max(0, r2[0])
    dx2 = min(r2[0] + r2[2], dst.shape[1])
    oy = dy - r2[1]
    ox = dx - r2[0]
    rh = dy2 - dy
    rw = dx2 - dx
    if rh <= 0 or rw <= 0:
        return
    region = dst[dy:dy2, dx:dx2]
    m = mask[oy:oy + rh, ox:ox + rw]
    w = warped[oy:oy + rh, ox:ox + rw]
    dst[dy:dy2, dx:dx2] = region * (1 - m) + w * m


# ── Morph generation ─────────────────────────────────────────────────────────


def morph_pair(img1, img2, pts1, pts2, triangles, t):
    """
    Produce a single morphed frame at blend position *t* (0 -> img1, 1 -> img2).
    *triangles* is a pre-computed list of (i, j, k) index triples.
    """
    p1 = np.float32(pts1)
    p2 = np.float32(pts2)
    pm = (1 - t) * p1 + t * p2

    f1 = img1.astype(np.float32)
    f2 = img2.astype(np.float32)
    morph1 = np.zeros_like(f1)
    morph2 = np.zeros_like(f2)

    for i, j, k in triangles:
        s1 = [tuple(p1[i]), tuple(p1[j]), tuple(p1[k])]
        s2 = [tuple(p2[i]), tuple(p2[j]), tuple(p2[k])]
        d = [tuple(pm[i]), tuple(pm[j]), tuple(pm[k])]
        warp_triangle(f1, s1, d, morph1)
        warp_triangle(f2, s2, d, morph2)

    blended = (1 - t) * morph1 + t * morph2
    return np.clip(blended, 0, 255).astype(np.uint8)


def crossfade_pair(img1, img2, t):
    """Simple alpha cross-dissolve fallback."""
    return cv2.addWeighted(img1, 1 - t, img2, t, 0)


# ── CLI ──────────────────────────────────────────────────────────────────────


def parse_size(value):
    """Parse a WIDTHxHEIGHT string into a (width, height) tuple."""
    try:
        w, h = value.lower().split("x")
        return int(w), int(h)
    except Exception:
        raise argparse.ArgumentTypeError(
            f"Invalid size '{value}'.  Expected WIDTHxHEIGHT (e.g. 1280x720)"
        )


def main():
    parser = argparse.ArgumentParser(
        description="Detect and align faces in a directory of photos, "
        "then assemble them into a video with smooth morph transitions."
    )
    parser.add_argument(
        "input_dir",
        type=Path,
        help="Directory containing input photos.",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=None,
        help="Output video path (default: <input_dir>/../output.mp4).",
    )
    parser.add_argument(
        "--fps",
        type=float,
        default=24,
        help="Video frame-rate (default: 24).",
    )
    parser.add_argument(
        "--duration",
        type=int,
        default=None,
        help="Milliseconds each source image is held before the next morph. "
        "If not set, each image is exactly one frame.",
    )
    parser.add_argument(
        "--morph-steps",
        type=int,
        default=0,
        help="Number of transition frames inserted between each consecutive "
        "pair of images (0 = no morphing, just a cut).",
    )
    parser.add_argument(
        "--size",
        type=parse_size,
        default=None,
        help="Force frame size WIDTHxHEIGHT (e.g. 1280x720).  "
        "Default: dimensions of the first processed image.",
    )
    parser.add_argument(
        "--save-frames",
        action="store_true",
        help="Also save individual aligned frames alongside the video.",
    )
    args = parser.parse_args()

    # ── validate paths ────────────────────────────────────────────────────
    input_dir = args.input_dir.resolve()
    if not input_dir.is_dir():
        parser.error(f"Input directory does not exist: {input_dir}")

    output_path = (
        args.output.resolve() if args.output else input_dir.parent / "output.mp4"
    )
    output_path.parent.mkdir(parents=True, exist_ok=True)

    if args.save_frames:
        frames_dir = output_path.parent / "frames"
        frames_dir.mkdir(parents=True, exist_ok=True)

    files = sorted(
        f
        for f in input_dir.iterdir()
        if f.suffix.lower() in (".jpg", ".jpeg", ".png", ".webp", ".bmp", ".tiff")
    )
    if not files:
        print(f"No image files found in {input_dir}")
        return

    hold_count = (
        max(1, round(args.fps * args.duration / 1000))
        if args.duration is not None
        else 1
    )

    # ── Phase 1: align every image (+ landmarks when morphing) ────────────
    print(f"Processing {len(files)} image(s) from {input_dir}")
    frame_size = args.size
    aligned = []  # list of (bgr_frame, landmarks, mtcnn_kps, used_dlib, filename)

    for idx, file in enumerate(files):
        print(f"[{idx + 1}/{len(files)}] {file.name}")
        result = align_face(file)
        if result is None:
            continue
        frame, mtcnn_kps = result

        if frame_size is None:
            frame_size = (frame.shape[1], frame.shape[0])
            print(f"Frame size (from first image): {frame_size[0]}x{frame_size[1]}")

        if (frame.shape[1], frame.shape[0]) != frame_size:
            frame = cv2.resize(frame, frame_size, interpolation=cv2.INTER_CUBIC)

        landmarks = None
        used_dlib = False
        if args.morph_steps > 0:
            landmarks, used_dlib = get_landmarks(frame, mtcnn_kps)

        aligned.append((frame, landmarks, mtcnn_kps, used_dlib, file.name))

        if args.save_frames:
            cv2.imwrite(str(frames_dir / f"c_{file.name}"), frame)
            print(f"  Saved frame: frames/c_{file.name}")

    if not aligned:
        print("No faces detected in any image - no video produced.")
        return

    # ── Phase 2: write video ──────────────────────────────────────────────
    print(f"")
    print(f"Output video : {output_path}")
    print(f"FPS          : {args.fps}")
    print(f"Hold frames  : {hold_count}  ({hold_count / args.fps * 1000:.0f} ms)")
    if args.morph_steps:
        print(
            f"Morph steps  : {args.morph_steps}  "
            f"({args.morph_steps / args.fps * 1000:.0f} ms)"
        )
    print(f"Frame size   : {frame_size[0]}x{frame_size[1]}")
    print()

    fourcc = cv2.VideoWriter_fourcc(*"mp4v")
    writer = cv2.VideoWriter(str(output_path), fourcc, args.fps, frame_size)
    if not writer.isOpened():
        print(f"Error: could not open video writer for {output_path}")
        return

    total_video_frames = 0

    try:
        for i, (frame, landmarks, mtcnn_kps, used_dlib, name) in enumerate(aligned):
            # Hold current frame
            for _ in range(hold_count):
                writer.write(frame)
                total_video_frames += 1

            # Morph transition to the next frame
            if args.morph_steps > 0 and i < len(aligned) - 1:
                next_frame, next_landmarks, next_mtcnn_kps, next_dlib, next_name = (
                    aligned[i + 1]
                )

                # When one side used dlib (68+8=76 pts) and the other fell
                # back to MTCNN (5+8=13 pts), the point counts differ and
                # indices can't be paired.  Downgrade both to MTCNN-only so
                # we always get a usable Delaunay morph.
                h, w = frame.shape[:2]
                boundary = [
                    (1, 1),
                    (w // 2, 1),
                    (w - 2, 1),
                    (1, h // 2),
                    (w - 2, h // 2),
                    (1, h - 2),
                    (w // 2, h - 2),
                    (w - 2, h - 2),
                ]
                if len(landmarks) != len(next_landmarks):
                    landmarks = list(mtcnn_kps) + boundary
                    next_landmarks = list(next_mtcnn_kps) + boundary

                # Compute triangulation once on the midpoint landmarks
                mid_pts = [
                    ((a[0] + b[0]) / 2.0, (a[1] + b[1]) / 2.0)
                    for a, b in zip(landmarks, next_landmarks)
                ]
                tri = compute_delaunay(mid_pts, frame_size)

                n_lm = len(landmarks) - 8  # exclude boundary points
                label = f"morph-{n_lm}pt"
                print(f"  {name} -> {next_name}  ({label}, {args.morph_steps} steps)")

                for step in range(1, args.morph_steps + 1):
                    t = step / (args.morph_steps + 1)
                    morphed = morph_pair(
                        frame,
                        next_frame,
                        landmarks,
                        next_landmarks,
                        tri,
                        t,
                    )
                    writer.write(morphed)
                    total_video_frames += 1
    finally:
        writer.release()

    duration_s = total_video_frames / args.fps
    print(
        f"Done - {total_video_frames} video frames, "
        f"{duration_s:.1f}s at {args.fps} fps -> {output_path}"
    )


if __name__ == "__main__":
    main()
