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
    detection.extended_corners = detection.corners;
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
    OverlayDrawsVisibleGrabLimitPolygon();
    HiddenOverlayDoesNotPolluteFrame();
    PartiallyOutOfFrameOverlayDoesNotCrash();
    DebugViewDrawsMaskCollisionPixels();
    NormalViewDoesNotDrawMaskCollisionPixels();

    std::cout << "frame_overlay_test passed" << std::endl;
    return 0;
}
