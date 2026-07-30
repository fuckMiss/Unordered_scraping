#include "frame_postprocess.h"
#include "YOLOv11_OBB.h"
#include "YOLOv11_SEG.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

constexpr int kImageSize = 220;
constexpr int kGripClass = 0;
constexpr int kBigClass = 1;
constexpr int kSmallClass = 2;

bool NearlyEqual(float left, float right)
{
    return std::fabs(left - right) < 0.01f;
}

float AngleBetween(cv::Point2f start, cv::Point2f end)
{
    float angle = std::atan2(end.y - start.y, end.x - start.x) * 180.0f / static_cast<float>(CV_PI);
    if (angle < 0.0f) {
        angle += 360.0f;
    }
    return angle;
}

OBBDetection MakeObb(int class_id, float conf, cv::Point2f center, cv::Size2f size, float angle)
{
    OBBDetection detection;
    detection.class_id = class_id;
    detection.conf = conf;
    detection.rotated_rect = cv::RotatedRect(center, size, angle);

    cv::Point2f points[4];
    detection.rotated_rect.points(points);
    detection.corners = { points[0], points[1], points[2], points[3] };
    return detection;
}

SegDetection MakeSegment(const cv::Rect& rect = cv::Rect(20, 20, 180, 180))
{
    SegDetection segment;
    segment.class_id = 0;
    segment.conf = 0.99f;
    segment.bbox = rect;
    segment.mask = cv::Mat(kImageSize, kImageSize, CV_8U, cv::Scalar(0));
    segment.mask(rect).setTo(cv::Scalar(255));
    return segment;
}

FrameInferenceResult RunPostprocess(const std::vector<OBBDetection>& obb_output,
                                    const std::vector<SegDetection>& seg_output = { MakeSegment() },
                                    FramePostprocessConfig config = {})
{
    return BuildFrameInferenceResult(obb_output,
                                     { "left", "big", "small" },
                                     seg_output,
                                     { "part" },
                                     kImageSize,
                                     kImageSize,
                                     1.0,
                                     1.0,
                                     config);
}

void BigRightMapsToHeadType1()
{
    const FrameInferenceResult result = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 140.0f, 110.0f }, { 20.0f, 10.0f }, 0.0f),
        MakeObb(kBigClass, 0.80f, { 165.0f, 80.0f }, { 16.0f, 8.0f }, 0.0f),
    });

    assert(result.pick_status_code == 1);
    assert(result.primary_index >= 0);
    assert(result.head_type_code == 1);
    assert(result.detections[result.primary_index].head_type_text == "大上右");
    assert(result.detections[result.primary_index].head_class_id == kBigClass);
    assert(result.detections[result.primary_index].can_grab);
}

void BigLeftMapsToHeadType3()
{
    const FrameInferenceResult result = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 70.0f, 110.0f }, { 20.0f, 10.0f }, 0.0f),
        MakeObb(kBigClass, 0.80f, { 45.0f, 80.0f }, { 16.0f, 8.0f }, 0.0f),
    });

    assert(result.pick_status_code == 1);
    assert(result.primary_index >= 0);
    assert(result.head_type_code == 3);
}

void SmallRightMapsToHeadType4()
{
    const FrameInferenceResult result = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 140.0f, 110.0f }, { 20.0f, 10.0f }, 0.0f),
        MakeObb(kSmallClass, 0.80f, { 165.0f, 80.0f }, { 16.0f, 8.0f }, 0.0f),
    });

    assert(result.pick_status_code == 1);
    assert(result.primary_index >= 0);
    assert(result.head_type_code == 4);
    assert(result.detections[result.primary_index].head_class_id == kSmallClass);
}

void SmallLeftMapsToHeadType2()
{
    const FrameInferenceResult result = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 70.0f, 110.0f }, { 20.0f, 10.0f }, 0.0f),
        MakeObb(kSmallClass, 0.80f, { 45.0f, 80.0f }, { 16.0f, 8.0f }, 0.0f),
    });

    assert(result.pick_status_code == 1);
    assert(result.primary_index >= 0);
    assert(result.head_type_code == 2);
}

void HeadOnlyRejects()
{
    const FrameInferenceResult result = RunPostprocess({
        MakeObb(kBigClass, 0.80f, { 120.0f, 120.0f }, { 20.0f, 10.0f }, 0.0f),
        MakeObb(kSmallClass, 0.80f, { 80.0f, 120.0f }, { 20.0f, 10.0f }, 0.0f),
    });

    assert(result.pick_status_code == 3);
    assert(result.primary_index == -1);
    assert(result.head_type_code == 0);
    assert(result.detections.empty());
    assert(result.raw_obb_regions.size() == 2);
}

void MissingHeadRejects()
{
    const FrameInferenceResult result = RunPostprocess({
        MakeObb(kGripClass, 0.80f, { 140.0f, 110.0f }, { 20.0f, 10.0f }, 0.0f),
    });

    assert(result.pick_status_code == 3);
    assert(result.primary_index == -1);
    assert(result.head_type_code == 0);
}

void GripMustMatchExactlyOneSegment()
{
    const FrameInferenceResult result = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 90.0f, 90.0f }, { 20.0f, 10.0f }, 0.0f),
        MakeObb(kBigClass, 0.80f, { 100.0f, 70.0f }, { 16.0f, 8.0f }, 0.0f),
    }, {
        MakeSegment(cv::Rect(20, 20, 120, 120)),
        MakeSegment(cv::Rect(60, 60, 120, 120)),
    });

    assert(result.pick_status_code == 3);
    assert(result.primary_index == -1);
}

void ExtendedGripTouchingOtherSegmentRejects()
{
    const FrameInferenceResult result = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 80.0f, 90.0f }, { 60.0f, 20.0f }, 0.0f),
        MakeObb(kSmallClass, 0.80f, { 70.0f, 70.0f }, { 16.0f, 8.0f }, 0.0f),
    }, {
        MakeSegment(cv::Rect(20, 20, 100, 120)),
        MakeSegment(cv::Rect(130, 20, 70, 120)),
    });

    assert(result.pick_status_code == 3);
    assert(result.primary_index == -1);
    assert(result.detections.size() == 1);
    assert(!result.detections.front().can_grab);
    assert(result.detections.front().extended_corners.size() == 4);
}

void SegmentWithBothBigAndSmallRejects()
{
    const FrameInferenceResult result = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 100.0f, 110.0f }, { 20.0f, 10.0f }, 0.0f),
        MakeObb(kBigClass, 0.80f, { 80.0f, 80.0f }, { 16.0f, 8.0f }, 0.0f),
        MakeObb(kSmallClass, 0.82f, { 125.0f, 80.0f }, { 16.0f, 8.0f }, 0.0f),
    });

    assert(result.pick_status_code == 3);
    assert(result.primary_index == -1);
    assert(result.detections.size() == 1);
    assert(!result.detections.front().can_grab);
}

void SegmentWithMultipleLeftRejects()
{
    const FrameInferenceResult result = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 90.0f, 110.0f }, { 20.0f, 10.0f }, 0.0f),
        MakeObb(kGripClass, 0.85f, { 130.0f, 110.0f }, { 20.0f, 10.0f }, 0.0f),
        MakeObb(kSmallClass, 0.80f, { 105.0f, 80.0f }, { 16.0f, 8.0f }, 0.0f),
    });

    assert(result.pick_status_code == 3);
    assert(result.primary_index == -1);
    assert(result.detections.size() == 2);
    assert(!result.detections[0].can_grab);
    assert(!result.detections[1].can_grab);
}

void MultipleCompleteTargetsChooseBestConfidence()
{
    const FrameInferenceResult result = RunPostprocess({
        MakeObb(kGripClass, 0.70f, { 70.0f, 85.0f }, { 20.0f, 10.0f }, 0.0f),
        MakeObb(kSmallClass, 0.80f, { 80.0f, 70.0f }, { 16.0f, 8.0f }, 0.0f),
        MakeObb(kGripClass, 0.95f, { 150.0f, 150.0f }, { 20.0f, 10.0f }, 0.0f),
        MakeObb(kBigClass, 0.80f, { 130.0f, 140.0f }, { 16.0f, 8.0f }, 0.0f),
    });

    assert(result.pick_status_code == 2);
    assert(result.primary_index >= 0);
    assert(result.detections[result.primary_index].confidence == 0.95f);
}

void FinalRayAngleMatchesGripObbRay()
{
    FramePostprocessConfig raw_config;
    raw_config.center_ray_offset_px = 0.0;
    const FrameInferenceResult raw = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 140.0f, 110.0f }, { 20.0f, 10.0f }, 0.0f),
        MakeObb(kBigClass, 0.80f, { 95.0f, 80.0f }, { 16.0f, 8.0f }, 0.0f),
    }, { MakeSegment(cv::Rect(20, 20, 180, 120)) }, raw_config);

    assert(raw.primary_index >= 0);
    const PoseDetection& target = raw.detections[raw.primary_index];
    assert(NearlyEqual(target.angle_deg, AngleBetween(target.obb_center, target.arrow_end)));
    assert(NearlyEqual(target.arrow_start.x, target.obb_center.x));
    assert(NearlyEqual(target.arrow_start.y, target.obb_center.y));
    assert(NearlyEqual(target.obb_center.x, 140.0f));
    assert(NearlyEqual(target.obb_center.y, 110.0f));
}

void GripObbRayDrivesFinalPose()
{
    const FrameInferenceResult result = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 120.0f, 110.0f }, { 20.0f, 10.0f }, 0.0f),
        MakeObb(kBigClass, 0.80f, { 120.0f, 110.0f }, { 16.0f, 8.0f }, 0.0f),
    }, { MakeSegment(cv::Rect(20, 20, 180, 180)) });

    assert(result.primary_index >= 0);
    const PoseDetection& target = result.detections[result.primary_index];
    assert(NearlyEqual(target.angle_deg, AngleBetween(target.obb_center, target.arrow_end)));
    assert(NearlyEqual(target.arrow_start.x, target.obb_center.x));
    assert(NearlyEqual(target.arrow_start.y, target.obb_center.y));
    assert(NearlyEqual(target.obb_center.x, 120.0f));
    assert(NearlyEqual(target.obb_center.y, 110.0f));
}

void RayTowardSegmentCenterFlipsAndOffsetsCenter()
{
    FramePostprocessConfig config;
    config.center_ray_offset_px = 10.0;
    const FrameInferenceResult result = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 110.0f, 110.0f }, { 20.0f, 10.0f }, 90.0f),
    }, { MakeSegment(cv::Rect(20, 20, 170, 180)) }, config);

    assert(result.detections.size() == 1);
    const PoseDetection& target = result.detections.front();
    assert(target.arrow_end.x > target.obb_center.x);
    assert(NearlyEqual(target.angle_deg, 0.0f));
    assert(NearlyEqual(target.center_x, 120.0f));
    assert(NearlyEqual(target.center_y, 110.0f));
}

void RayDoesNotFlipWhenSegmentCenterIsNotOnForwardDisplayRay()
{
    const FrameInferenceResult reverse_direction = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 110.0f, 110.0f }, { 20.0f, 10.0f }, 90.0f),
    }, { MakeSegment(cv::Rect(31, 20, 170, 180)) });
    const FrameInferenceResult beyond_display_length = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 110.0f, 110.0f }, { 20.0f, 10.0f }, 90.0f),
    }, { MakeSegment(cv::Rect(20, 20, 120, 180)) });
    const FrameInferenceResult outside_tolerance = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 110.0f, 110.0f }, { 20.0f, 10.0f }, 90.0f),
    }, { MakeSegment(cv::Rect(20, 30, 170, 180)) });

    assert(reverse_direction.detections.size() == 1);
    assert(beyond_display_length.detections.size() == 1);
    assert(outside_tolerance.detections.size() == 1);
    assert(reverse_direction.detections.front().arrow_end.x < reverse_direction.detections.front().obb_center.x);
    assert(beyond_display_length.detections.front().arrow_end.x < beyond_display_length.detections.front().obb_center.x);
    assert(outside_tolerance.detections.front().arrow_end.x < outside_tolerance.detections.front().obb_center.x);
}

void FlippedRayDrivesHeadCollisionDecision()
{
    const FrameInferenceResult result = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 110.0f, 110.0f }, { 20.0f, 10.0f }, 90.0f),
        MakeObb(kBigClass, 0.80f, { 112.0f, 110.0f }, { 16.0f, 8.0f }, 0.0f),
    }, { MakeSegment(cv::Rect(20, 18, 150, 180)) });

    assert(result.detections.size() == 1);
    const PoseDetection& target = result.detections.front();
    assert(target.arrow_end.x > target.obb_center.x);
    assert(!target.can_grab);
    assert(result.primary_index == -1);
    assert(result.pick_status_code == 3);
}

} // namespace

int main()
{
    BigRightMapsToHeadType1();
    BigLeftMapsToHeadType3();
    SmallRightMapsToHeadType4();
    SmallLeftMapsToHeadType2();
    HeadOnlyRejects();
    MissingHeadRejects();
    GripMustMatchExactlyOneSegment();
    ExtendedGripTouchingOtherSegmentRejects();
    SegmentWithBothBigAndSmallRejects();
    SegmentWithMultipleLeftRejects();
    MultipleCompleteTargetsChooseBestConfidence();
    FinalRayAngleMatchesGripObbRay();
    GripObbRayDrivesFinalPose();
    RayTowardSegmentCenterFlipsAndOffsetsCenter();
    RayDoesNotFlipWhenSegmentCenterIsNotOnForwardDisplayRay();
    FlippedRayDrivesHeadCollisionDecision();

    std::cout << "frame_postprocess_smoke passed" << std::endl;
    return 0;
}
