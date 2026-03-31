#include "face_align.h"
#include <dlib/opencv.h>
#include <opencv2/imgproc.hpp>
#include <cmath>
#include <iostream>

namespace core {

/// Helper: average the x,y coordinates of shape parts in [begin, end].
static cv::Point2f averageParts(const dlib::full_object_detection& shape,
                                int begin, int end)
{
    float sx = 0.0f, sy = 0.0f;
    int n = end - begin + 1;
    for (int i = begin; i <= end; ++i) {
        sx += static_cast<float>(shape.part(i).x());
        sy += static_cast<float>(shape.part(i).y());
    }
    return {sx / n, sy / n};
}

/// Transform a 2D point through a 2×3 affine matrix (CV_64F).
static cv::Point2f transformPoint(const cv::Mat& M, float x, float y)
{
    double tx = M.at<double>(0, 0) * x + M.at<double>(0, 1) * y + M.at<double>(0, 2);
    double ty = M.at<double>(1, 0) * x + M.at<double>(1, 1) * y + M.at<double>(1, 2);
    return {static_cast<float>(tx), static_cast<float>(ty)};
}

std::optional<AlignedFace> alignFace(
    const cv::Mat& bgr_image,
    const std::string& filename,
    dlib::frontal_face_detector& detector,
    dlib::shape_predictor& predictor)
{
    // --- 1. Convert BGR → RGB for dlib ----------------------------------
    cv::Mat rgb;
    cv::cvtColor(bgr_image, rgb, cv::COLOR_BGR2RGB);
    dlib::cv_image<dlib::rgb_pixel> dlib_img(rgb);

    // --- 2. Detect faces ------------------------------------------------
    std::vector<dlib::rectangle> faces = detector(dlib_img);
    if (faces.empty()) {
        std::cout << "  [align] No face detected in " << filename << "\n";
        return std::nullopt;
    }

    // --- 3. Get 68 landmarks on the first (largest / most-confident) face
    dlib::full_object_detection shape = predictor(dlib_img, faces[0]);

    // --- 4. Derive 5 keypoints from the 68-point model ------------------
    //   left  eye centre : mean of parts 36-41
    //   right eye centre : mean of parts 42-47
    //   nose tip         : part 30
    //   mouth left       : part 48
    //   mouth right      : part 54
    cv::Point2f leftEyeCenter  = averageParts(shape, 36, 41);
    cv::Point2f rightEyeCenter = averageParts(shape, 42, 47);
    cv::Point2f noseTip(static_cast<float>(shape.part(30).x()),
                        static_cast<float>(shape.part(30).y()));
    cv::Point2f mouthLeft(static_cast<float>(shape.part(48).x()),
                          static_cast<float>(shape.part(48).y()));
    cv::Point2f mouthRight(static_cast<float>(shape.part(54).x()),
                           static_cast<float>(shape.part(54).y()));

    // --- 5. Alignment math (mirrors the Python reference) ---------------
    double dY = static_cast<double>(rightEyeCenter.y - leftEyeCenter.y);
    double dX = static_cast<double>(rightEyeCenter.x - leftEyeCenter.x);
    double angle = std::atan2(dY, dX) * 180.0 / CV_PI; // degrees

    constexpr double desiredLeftEyeX  = 0.35;
    constexpr double desiredLeftEyeY  = 0.35;
    constexpr double desiredRightEyeX = 1.0 - desiredLeftEyeX;

    int desiredFaceWidth  = bgr_image.cols;
    int desiredFaceHeight = bgr_image.rows;

    double dist        = std::sqrt(dX * dX + dY * dY);
    double desiredDist = (desiredRightEyeX - desiredLeftEyeX) * 300.0;
    double scale       = desiredDist / dist;

    // Eyes centre (integer division, matching Python's // 2)
    cv::Point2f eyesCenter(
        static_cast<float>(static_cast<int>(leftEyeCenter.x + rightEyeCenter.x) / 2),
        static_cast<float>(static_cast<int>(leftEyeCenter.y + rightEyeCenter.y) / 2));

    // --- 6. Build the affine matrix -------------------------------------
    cv::Mat M = cv::getRotationMatrix2D(eyesCenter, angle, scale); // 2×3 CV_64F

    // Translate so that the face is centred in the output canvas
    M.at<double>(0, 2) += desiredFaceWidth * 0.5 - eyesCenter.x;
    M.at<double>(1, 2) += desiredFaceHeight * desiredLeftEyeY - eyesCenter.y;

    // --- 7. Warp the BGR image (keep BGR for OpenCV compatibility) ------
    cv::Mat output;
    cv::warpAffine(bgr_image, output,
                   M,
                   cv::Size(desiredFaceWidth, desiredFaceHeight),
                   cv::INTER_CUBIC,
                   cv::BORDER_CONSTANT);

    // --- 8. Transform the 5 keypoints through M -------------------------
    FaceKeypoints kps;
    kps.left_eye    = transformPoint(M, leftEyeCenter.x,  leftEyeCenter.y);
    kps.right_eye   = transformPoint(M, rightEyeCenter.x, rightEyeCenter.y);
    kps.nose        = transformPoint(M, noseTip.x,         noseTip.y);
    kps.mouth_left  = transformPoint(M, mouthLeft.x,       mouthLeft.y);
    kps.mouth_right = transformPoint(M, mouthRight.x,      mouthRight.y);

    // --- 9. Return result -----------------------------------------------
    return AlignedFace{std::move(output), kps, filename};
}

} // namespace core