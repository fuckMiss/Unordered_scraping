#include "frame_overlay.h"

#include <opencv2/opencv.hpp>

#include <cassert>
#include <iostream>

namespace {

bool HasNonBlackPixel(const cv::Mat& image)
{
    for (int y = 0; y < image.rows; ++y) {
        for (int x = 0; x < image.cols; ++x) {
            if (image.at<cv::Vec3b>(y, x) != cv::Vec3b(0, 0, 0)) {
                return true;
            }
        }
    }
    return false;
}

int CountNonBlackPixels(const cv::Mat& image, const cv::Rect& roi)
{
    int count = 0;
    const cv::Rect clipped = roi & cv::Rect(0, 0, image.cols, image.rows);
    for (int y = clipped.y; y < clipped.y + clipped.height; ++y) {
        for (int x = clipped.x; x < clipped.x + clipped.width; ++x) {
            if (image.at<cv::Vec3b>(y, x) != cv::Vec3b(0, 0, 0)) {
                ++count;
            }
        }
    }
    return count;
}

PoseDetection MakeDisplayDetection(int matched_segment_index,
                                   float confidence,
                                   bool can_grab,
                                   cv::Point2f center)
{
    PoseDetection detection;
    detection.class_id = 0;
    detection.matched_segment_index = matched_segment_index;
    detection.confidence = confidence;
    detection.can_grab = can_grab;
    detection.obb_center = center;
    detection.arrow_start = center;
    detection.arrow_end = center + cv::Point2f(10.0f, 0.0f);
    detection.corners = {
        center + cv::Point2f(-5.0f, -5.0f),
        center + cv::Point2f(5.0f, -5.0f),
        center + cv::Point2f(5.0f, 5.0f),
        center + cv::Point2f(-5.0f, 5.0f),
    };
    return detection;
}

void NormalDisplaySelectionUsesPrimaryThenGrabbableThenConfidence()
{
    FrameInferenceResult result;
    result.primary_index = 1;
    result.detections.push_back(MakeDisplayDetection(0, 0.99f, true, { 20.0f, 20.0f }));
    result.detections.push_back(MakeDisplayDetection(0, 0.50f, false, { 60.0f, 20.0f }));
    result.detections.push_back(MakeDisplayDetection(1, 0.70f, false, { 20.0f, 60.0f }));
    result.detections.push_back(MakeDisplayDetection(1, 0.80f, true, { 60.0f, 60.0f }));
    result.detections.push_back(MakeDisplayDetection(2, 0.60f, false, { 20.0f, 100.0f }));
    result.detections.push_back(MakeDisplayDetection(2, 0.90f, false, { 60.0f, 100.0f }));

    const std::vector<int> indices = SelectNormalDisplayDetectionIndices(result);
    assert(indices.size() == 3);
    assert(indices[0] == 1);
    assert(indices[1] == 3);
    assert(indices[2] == 5);
}

void NormalViewDrawsOneDetectionPerSegment()
{
    cv::Mat image(80, 100, CV_8UC3, cv::Scalar(0, 0, 0));
    FrameInferenceResult result;
    result.display_scale = 1.0;
    result.detections.push_back(MakeDisplayDetection(0, 0.40f, false, { 20.0f, 30.0f }));
    result.detections.push_back(MakeDisplayDetection(0, 0.90f, false, { 70.0f, 30.0f }));

    DrawFrameOverlay(image, result, -1, false, false, false, true, nullptr);

    assert(CountNonBlackPixels(image, cv::Rect(10, 20, 20, 20)) == 0);
    assert(CountNonBlackPixels(image, cv::Rect(60, 20, 20, 20)) > 0);
}

void NormalViewDrawsDifferentSegments()
{
    cv::Mat image(80, 100, CV_8UC3, cv::Scalar(0, 0, 0));
    FrameInferenceResult result;
    result.display_scale = 1.0;
    result.detections.push_back(MakeDisplayDetection(0, 0.40f, false, { 20.0f, 30.0f }));
    result.detections.push_back(MakeDisplayDetection(1, 0.90f, false, { 70.0f, 30.0f }));

    DrawFrameOverlay(image, result, -1, false, false, false, true, nullptr);

    assert(CountNonBlackPixels(image, cv::Rect(10, 20, 20, 20)) > 0);
    assert(CountNonBlackPixels(image, cv::Rect(60, 20, 20, 20)) > 0);
}

void DebugViewStillDrawsDuplicateSegmentDetections()
{
    cv::Mat image(80, 100, CV_8UC3, cv::Scalar(0, 0, 0));
    FrameInferenceResult result;
    result.display_scale = 1.0;
    result.detections.push_back(MakeDisplayDetection(0, 0.40f, false, { 20.0f, 30.0f }));
    result.detections.push_back(MakeDisplayDetection(0, 0.90f, false, { 70.0f, 30.0f }));

    DrawFrameOverlay(image, result, -1, false, true, false, true, nullptr);

    assert(CountNonBlackPixels(image, cv::Rect(10, 20, 20, 20)) > 0);
    assert(CountNonBlackPixels(image, cv::Rect(60, 20, 20, 20)) > 0);
}

void MechanicalGripperOverlayDrawsConfiguredRectangle()
{
    cv::Mat image(80, 100, CV_8UC3, cv::Scalar(0, 0, 0));
    FrameInferenceResult result;
    result.display_scale = 1.0;
    result.primary_index = 0;
    PoseDetection detection = MakeDisplayDetection(0, 0.95f, true, { 50.0f, 40.0f });
    detection.mechanical_gripper_corners = {
        { 35.0f, 32.0f },
        { 65.0f, 32.0f },
        { 65.0f, 48.0f },
        { 35.0f, 48.0f },
    };
    result.detections.push_back(detection);

    DrawFrameOverlay(image, result, -1, false, false, false, true, nullptr);

    const cv::Vec3b pixel = image.at<cv::Vec3b>(32, 50);
    assert(pixel[2] > pixel[0]);
    assert(pixel[1] > pixel[0]);
}

void MechanicalGripperOverlayUsesRejectedStateColor()
{
    cv::Mat image(80, 100, CV_8UC3, cv::Scalar(0, 0, 0));
    FrameInferenceResult result;
    result.display_scale = 1.0;
    PoseDetection detection = MakeDisplayDetection(0, 0.95f, false, { 50.0f, 40.0f });
    detection.mechanical_gripper_corners = {
        { 35.0f, 32.0f },
        { 65.0f, 32.0f },
        { 65.0f, 48.0f },
        { 35.0f, 48.0f },
    };
    result.detections.push_back(detection);

    DrawFrameOverlay(image, result, -1, false, false, false, true, nullptr);

    const cv::Vec3b pixel = image.at<cv::Vec3b>(32, 50);
    assert(pixel[2] > pixel[1]);
    assert(pixel[2] > pixel[0]);
}

void NormalViewDrawsPlcCommandPoseWhenAvailable()
{
    cv::Mat image(120, 140, CV_8UC3, cv::Scalar(0, 0, 0));
    FrameInferenceResult result;
    result.display_scale = 1.0;
    result.primary_index = 0;
    PoseDetection detection = MakeDisplayDetection(0, 0.95f, true, { 30.0f, 40.0f });
    detection.angle_deg = 0.0f;
    detection.mechanical_gripper_corners = {
        { 16.0f, 30.0f },
        { 44.0f, 30.0f },
        { 44.0f, 50.0f },
        { 16.0f, 50.0f },
    };
    detection.center_x = 30.0f;
    detection.center_y = 40.0f;
    detection.has_plc_command_pose = true;
    detection.plc_command_center = { 80.0f, 40.0f };
    detection.plc_command_arrow_start = { 80.0f, 40.0f };
    detection.plc_command_arrow_end = { 80.0f, 55.0f };
    detection.plc_command_angle_deg = 90.0f;
    result.detections.push_back(detection);

    DrawFrameOverlay(image, result, -1, false, false, false, true, nullptr);

    assert(CountNonBlackPixels(image, cv::Rect(18, 30, 24, 20)) == 0);
    assert(CountNonBlackPixels(image, cv::Rect(68, 26, 26, 28)) > 0);
    assert(CountNonBlackPixels(image, cv::Rect(74, 54, 12, 30)) > 0);
    assert(CountNonBlackPixels(image, cv::Rect(100, 34, 20, 12)) == 0);
}

void NormalViewAlignsBaseObbToMechanicalGripper()
{
    cv::Mat image(120, 140, CV_8UC3, cv::Scalar(0, 0, 0));
    FrameInferenceResult result;
    result.display_scale = 1.0;
    result.primary_index = 0;
    PoseDetection detection = MakeDisplayDetection(0, 0.95f, true, { 70.0f, 60.0f });
    detection.center_x = 70.0f;
    detection.center_y = 60.0f;
    detection.corners = {
        { 65.0f, 40.0f },
        { 75.0f, 40.0f },
        { 75.0f, 80.0f },
        { 65.0f, 80.0f },
    };
    detection.mechanical_gripper_corners = {
        { 50.0f, 55.0f },
        { 90.0f, 55.0f },
        { 90.0f, 65.0f },
        { 50.0f, 65.0f },
    };
    result.detections.push_back(detection);

    DrawFrameOverlay(image, result, -1, false, false, false, true, nullptr);

    assert(CountNonBlackPixels(image, cv::Rect(62, 38, 16, 8)) == 0);
    assert(CountNonBlackPixels(image, cv::Rect(48, 52, 44, 16)) > 0);
}

void DebugViewDrawsRawObbOnceAndCommandPoseWhenCommandPoseExists()
{
    cv::Mat image(80, 120, CV_8UC3, cv::Scalar(0, 0, 0));
    FrameInferenceResult result;
    result.display_scale = 1.0;
    result.primary_index = 0;
    PoseDetection detection = MakeDisplayDetection(0, 0.95f, true, { 30.0f, 40.0f });
    detection.mechanical_gripper_corners = {
        { 20.0f, 32.0f },
        { 40.0f, 32.0f },
        { 40.0f, 48.0f },
        { 20.0f, 48.0f },
    };
    detection.center_x = 30.0f;
    detection.center_y = 40.0f;
    detection.has_plc_command_pose = true;
    detection.plc_command_center = { 80.0f, 40.0f };
    detection.plc_command_arrow_start = { 80.0f, 40.0f };
    detection.plc_command_arrow_end = { 95.0f, 40.0f };
    result.detections.push_back(detection);
    ObbRegion raw;
    raw.class_id = detection.class_id;
    raw.center = detection.obb_center;
    raw.corners = detection.corners;
    result.raw_obb_regions.push_back(raw);

    DrawFrameOverlay(image, result, -1, false, true, false, true, nullptr);

    assert(CountNonBlackPixels(image, cv::Rect(18, 30, 24, 20)) > 0);
    assert(CountNonBlackPixels(image, cv::Rect(68, 30, 24, 20)) > 0);
}

void OverlayDrawsVisibleGrabLimitPolygon()
{
    cv::Mat image(120, 160, CV_8UC3, cv::Scalar(0, 0, 0));
    FrameInferenceResult result;
    result.display_scale = 1.0;

    GrabLimitOverlayView overlay;
    overlay.visible = true;
    overlay.message = "Grab ROI";
    overlay.polygon = {
        { 30.0f, 30.0f },
        { 130.0f, 30.0f },
        { 130.0f, 90.0f },
        { 30.0f, 90.0f },
    };

    DrawFrameOverlay(image, result, -1, false, false, false, true, &overlay);
    assert(HasNonBlackPixel(image));
    assert(image.at<cv::Vec3b>(5, 5) == cv::Vec3b(0, 0, 0));
}

void HiddenOverlayDoesNotPolluteFrame()
{
    cv::Mat image(80, 100, CV_8UC3, cv::Scalar(0, 0, 0));
    FrameInferenceResult result;
    GrabLimitOverlayView overlay;
    overlay.visible = false;

    DrawFrameOverlay(image, result, -1, false, false, false, true, &overlay);
    assert(!HasNonBlackPixel(image));
}

void PartiallyOutOfFrameOverlayDoesNotCrash()
{
    cv::Mat image(80, 100, CV_8UC3, cv::Scalar(0, 0, 0));
    FrameInferenceResult result;
    GrabLimitOverlayView overlay;
    overlay.visible = true;
    overlay.message = "Grab ROI";
    overlay.polygon = {
        { -40.0f, -20.0f },
        { 80.0f, -10.0f },
        { 120.0f, 50.0f },
        { -20.0f, 70.0f },
    };

    DrawFrameOverlay(image, result, -1, false, false, false, true, &overlay);
    assert(HasNonBlackPixel(image));
}

void DebugViewDrawsMaskCollisionPixels()
{
    cv::Mat image(80, 100, CV_8UC3, cv::Scalar(0, 0, 0));
    FrameInferenceResult result;
    result.display_scale = 1.0;
    PoseDetection detection;
    cv::Mat collision_mask(4, 4, CV_8U, cv::Scalar(255));
    detection.mask_collisions.push_back({ cv::Rect(30, 30, 4, 4), collision_mask });
    result.detections.push_back(detection);

    DrawFrameOverlay(image, result, -1, false, true, false, true, nullptr);

    const cv::Vec3b pixel = image.at<cv::Vec3b>(31, 31);
    assert(pixel[2] > pixel[1]);
    assert(pixel[2] > pixel[0]);
}

void NormalViewDoesNotDrawMaskCollisionPixels()
{
    cv::Mat image(80, 100, CV_8UC3, cv::Scalar(0, 0, 0));
    FrameInferenceResult result;
    result.display_scale = 1.0;
    PoseDetection detection;
    cv::Mat collision_mask(4, 4, CV_8U, cv::Scalar(255));
    detection.mask_collisions.push_back({ cv::Rect(30, 30, 4, 4), collision_mask });
    result.detections.push_back(detection);

    DrawFrameOverlay(image, result, -1, false, false, false, true, nullptr);

    assert(!HasNonBlackPixel(image));
}

} // namespace

int main()
{
    NormalDisplaySelectionUsesPrimaryThenGrabbableThenConfidence();
    NormalViewDrawsOneDetectionPerSegment();
    NormalViewDrawsDifferentSegments();
    DebugViewStillDrawsDuplicateSegmentDetections();
    MechanicalGripperOverlayDrawsConfiguredRectangle();
    MechanicalGripperOverlayUsesRejectedStateColor();
    NormalViewDrawsPlcCommandPoseWhenAvailable();
    NormalViewAlignsBaseObbToMechanicalGripper();
    DebugViewDrawsRawObbOnceAndCommandPoseWhenCommandPoseExists();
    OverlayDrawsVisibleGrabLimitPolygon();
    HiddenOverlayDoesNotPolluteFrame();
    PartiallyOutOfFrameOverlayDoesNotCrash();
    DebugViewDrawsMaskCollisionPixels();
    NormalViewDoesNotDrawMaskCollisionPixels();

    std::cout << "frame_overlay_test passed" << std::endl;
    return 0;
}
