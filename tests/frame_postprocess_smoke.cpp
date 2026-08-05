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

SegDetection MakeSegmentWithMask(const cv::Rect& bbox, const std::vector<cv::Rect>& mask_rects)
{
    SegDetection segment;
    segment.class_id = 0;
    segment.conf = 0.99f;
    segment.bbox = bbox;
    segment.mask = cv::Mat(kImageSize, kImageSize, CV_8U, cv::Scalar(0));
    const cv::Rect image_rect(0, 0, kImageSize, kImageSize);
    for (const cv::Rect& rect : mask_rects) {
        const cv::Rect clipped = rect & image_rect;
        if (!clipped.empty()) {
            segment.mask(clipped).setTo(cv::Scalar(255));
        }
    }
    return segment;
}

SegDetection MakeEmptyMaskSegment(const cv::Rect& bbox)
{
    SegDetection segment;
    segment.class_id = 0;
    segment.conf = 0.99f;
    segment.bbox = bbox;
    segment.mask = cv::Mat();
    return segment;
}

SegDetection MakeWrongSizeMaskSegment(const cv::Rect& bbox)
{
    SegDetection segment;
    segment.class_id = 0;
    segment.conf = 0.99f;
    segment.bbox = bbox;
    segment.mask = cv::Mat(20, 20, CV_8U, cv::Scalar(255));
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

void BigHeadBelowRightGripMapsToLocalLeft()
{
    const FrameInferenceResult result = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 140.0f, 110.0f }, { 20.0f, 10.0f }, 0.0f),
        MakeObb(kBigClass, 0.80f, { 140.0f, 150.0f }, { 16.0f, 8.0f }, 0.0f),
    });

    assert(result.pick_status_code == 1);
    assert(result.primary_index >= 0);
    assert(result.detections[result.primary_index].angle_deg < 1.0f);
    assert(result.head_type_code == 3);
    assert(result.detections[result.primary_index].head_type_text == "大上左");
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

void ExtendedGripTouchingOtherSegmentNoLongerRejectsInPostprocess()
{
    const FrameInferenceResult result = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 80.0f, 90.0f }, { 60.0f, 20.0f }, 0.0f),
        MakeObb(kSmallClass, 0.80f, { 70.0f, 70.0f }, { 16.0f, 8.0f }, 0.0f),
    }, {
        MakeSegment(cv::Rect(20, 20, 100, 120)),
        MakeSegment(cv::Rect(130, 20, 70, 120)),
    });

    assert(result.pick_status_code == 1);
    assert(result.primary_index == 0);
    assert(result.detections.size() == 1);
    assert(result.detections.front().can_grab);
    assert(result.detections.front().mask_collisions.empty());
}

void ExtendedGripOnlyTouchingOtherSegmentBboxStaysGrabbable()
{
    const FrameInferenceResult result = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 80.0f, 90.0f }, { 60.0f, 20.0f }, 0.0f),
        MakeObb(kSmallClass, 0.80f, { 70.0f, 70.0f }, { 16.0f, 8.0f }, 0.0f),
    }, {
        MakeSegment(cv::Rect(20, 20, 100, 120)),
        MakeSegmentWithMask(cv::Rect(130, 20, 70, 120), { cv::Rect(180, 20, 20, 20) }),
    });

    assert(result.pick_status_code == 1);
    assert(result.primary_index == 0);
    assert(result.detections.size() == 1);
    assert(result.detections.front().can_grab);
    assert(result.detections.front().mask_collisions.empty());
}

void ExtendedGripThroughMaskHoleStaysGrabbable()
{
    const FrameInferenceResult result = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 80.0f, 90.0f }, { 60.0f, 20.0f }, 0.0f),
        MakeObb(kSmallClass, 0.80f, { 70.0f, 70.0f }, { 16.0f, 8.0f }, 0.0f),
    }, {
        MakeSegment(cv::Rect(20, 20, 100, 120)),
        MakeSegmentWithMask(cv::Rect(130, 20, 70, 120), {
            cv::Rect(130, 20, 70, 20),
            cv::Rect(130, 120, 70, 20),
            cv::Rect(180, 20, 20, 120),
        }),
    });

    assert(result.pick_status_code == 1);
    assert(result.primary_index == 0);
    assert(result.detections.front().can_grab);
    assert(result.detections.front().mask_collisions.empty());
}

void NeighborMaskInsideMatchedMaskStaysGrabbable()
{
    const FrameInferenceResult result = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 80.0f, 90.0f }, { 60.0f, 20.0f }, 0.0f),
        MakeObb(kSmallClass, 0.80f, { 70.0f, 70.0f }, { 16.0f, 8.0f }, 0.0f),
    }, {
        MakeSegment(cv::Rect(20, 20, 100, 120)),
        MakeSegmentWithMask(cv::Rect(100, 85, 40, 30), { cv::Rect(100, 85, 10, 10) }),
    });

    assert(result.pick_status_code == 1);
    assert(result.primary_index == 0);
    assert(result.detections.size() == 1);
    assert(result.detections.front().can_grab);
    assert(result.detections.front().mask_collisions.empty());
}

void ExpandedGripCollisionNoLongerRejectsInPostprocess()
{
    const FrameInferenceResult result = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 80.0f, 90.0f }, { 60.0f, 20.0f }, 0.0f),
        MakeObb(kSmallClass, 0.80f, { 70.0f, 70.0f }, { 16.0f, 8.0f }, 0.0f),
        MakeObb(kGripClass, 0.88f, { 185.0f, 160.0f }, { 20.0f, 10.0f }, 0.0f),
        MakeObb(kSmallClass, 0.82f, { 170.0f, 145.0f }, { 16.0f, 8.0f }, 0.0f),
    }, {
        MakeSegment(cv::Rect(20, 20, 100, 120)),
        MakeSegmentWithMask(cv::Rect(150, 85, 60, 115), {
            cv::Rect(150, 85, 10, 10),
            cv::Rect(160, 130, 50, 70),
        }),
    });

    assert(result.pick_status_code == 2);
    assert(result.detections.size() == 2);

    const PoseDetection* sweeping_target = nullptr;
    const PoseDetection* swept_target = nullptr;
    for (const PoseDetection& detection : result.detections) {
        if (NearlyEqual(detection.obb_center.x, 80.0f)) {
            sweeping_target = &detection;
        } else if (NearlyEqual(detection.obb_center.x, 185.0f)) {
            swept_target = &detection;
        }
    }

    assert(sweeping_target != nullptr);
    assert(swept_target != nullptr);
    assert(sweeping_target->can_grab);
    assert(sweeping_target->mask_collisions.empty());
    assert(swept_target->can_grab);
    assert(swept_target->mask_collisions.empty());
    assert(result.detections[result.primary_index].obb_center.x == sweeping_target->obb_center.x);
}

void EmptyOrWrongSizeNeighborMaskDoesNotFallbackToBbox()
{
    const FrameInferenceResult empty_mask = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 80.0f, 90.0f }, { 60.0f, 20.0f }, 0.0f),
        MakeObb(kSmallClass, 0.80f, { 70.0f, 70.0f }, { 16.0f, 8.0f }, 0.0f),
    }, {
        MakeSegment(cv::Rect(20, 20, 100, 120)),
        MakeEmptyMaskSegment(cv::Rect(130, 20, 70, 120)),
    });
    assert(empty_mask.pick_status_code == 1);
    assert(empty_mask.detections.front().mask_collisions.empty());

    const FrameInferenceResult wrong_size_mask = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 80.0f, 90.0f }, { 60.0f, 20.0f }, 0.0f),
        MakeObb(kSmallClass, 0.80f, { 70.0f, 70.0f }, { 16.0f, 8.0f }, 0.0f),
    }, {
        MakeSegment(cv::Rect(20, 20, 100, 120)),
        MakeWrongSizeMaskSegment(cv::Rect(130, 20, 70, 120)),
    });
    assert(wrong_size_mask.pick_status_code == 1);
    assert(wrong_size_mask.detections.front().mask_collisions.empty());
}

void MultipleNeighborMasksDoNotRejectInPostprocess()
{
    const FrameInferenceResult result = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 80.0f, 90.0f }, { 60.0f, 20.0f }, 0.0f),
        MakeObb(kSmallClass, 0.80f, { 70.0f, 70.0f }, { 16.0f, 8.0f }, 0.0f),
    }, {
        MakeSegment(cv::Rect(20, 20, 100, 120)),
        MakeSegmentWithMask(cv::Rect(130, 20, 70, 120), { cv::Rect(130, 85, 10, 10) }),
        MakeSegmentWithMask(cv::Rect(145, 20, 70, 120), { cv::Rect(145, 85, 10, 10) }),
    });

    assert(result.pick_status_code == 1);
    assert(result.primary_index == 0);
    assert(result.detections.front().can_grab);
    assert(result.detections.front().mask_collisions.empty());
}

void MultipleHeadCandidatesChooseClosestRightAngle()
{
    const FrameInferenceResult result = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 140.0f, 110.0f }, { 20.0f, 10.0f }, 90.0f),
        MakeObb(kSmallClass, 0.95f, { 150.0f, 80.0f }, { 16.0f, 8.0f }, 0.0f),
        MakeObb(kBigClass, 0.70f, { 110.0f, 80.0f }, { 16.0f, 8.0f }, 0.0f),
    });

    assert(result.pick_status_code == 1);
    assert(result.primary_index >= 0);
    assert(result.detections.size() == 1);
    assert(result.detections.front().can_grab);
    assert(result.detections.front().head_class_id == kBigClass);
    assert(result.head_type_code == 1);
}

void EqualHeadCandidatesUseStableTieBreak()
{
    const FrameInferenceResult result = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 140.0f, 110.0f }, { 20.0f, 10.0f }, 90.0f),
        MakeObb(kSmallClass, 0.80f, { 140.0f, 80.0f }, { 16.0f, 8.0f }, 0.0f),
        MakeObb(kBigClass, 0.80f, { 140.0f, 80.0f }, { 16.0f, 8.0f }, 0.0f),
    });

    assert(result.pick_status_code == 1);
    assert(result.primary_index >= 0);
    assert(result.detections.front().head_class_id == kBigClass);
    assert(result.head_type_code == 1);
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
    }, {
        MakeSegment(cv::Rect(20, 20, 90, 90)),
        MakeSegment(cv::Rect(120, 120, 80, 80)),
    });

    assert(result.pick_status_code == 2);
    assert(result.primary_index >= 0);
    assert(result.detections[result.primary_index].confidence == 0.95f);
}

void NearTieCompleteTargetsChooseStableSegmentOrder()
{
    const FrameInferenceResult result = RunPostprocess({
        MakeObb(kGripClass, 0.90005f, { 150.0f, 150.0f }, { 20.0f, 10.0f }, 0.0f),
        MakeObb(kBigClass, 0.80f, { 130.0f, 140.0f }, { 16.0f, 8.0f }, 0.0f),
        MakeObb(kGripClass, 0.90000f, { 70.0f, 85.0f }, { 20.0f, 10.0f }, 0.0f),
        MakeObb(kSmallClass, 0.80f, { 80.0f, 70.0f }, { 16.0f, 8.0f }, 0.0f),
    }, {
        MakeSegment(cv::Rect(20, 20, 90, 90)),
        MakeSegment(cv::Rect(120, 120, 80, 80)),
    });

    assert(result.pick_status_code == 2);
    assert(result.primary_index >= 0);
    assert(result.detections[result.primary_index].matched_segment_index == 0);
}

void FinalPoseOutputIsQuantized()
{
    const FrameInferenceResult result = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 110.004f, 110.006f }, { 20.0f, 10.0f }, 90.0f),
        MakeObb(kBigClass, 0.80f, { 80.0f, 80.0f }, { 16.0f, 8.0f }, 0.0f),
    }, { MakeSegment(cv::Rect(20, 20, 120, 180)) });

    assert(result.primary_index >= 0);
    const PoseDetection& target = result.detections[result.primary_index];
    assert(NearlyEqual(target.center_x * 100.0f, std::round(target.center_x * 100.0f)));
    assert(NearlyEqual(target.center_y * 100.0f, std::round(target.center_y * 100.0f)));
    assert(NearlyEqual(target.angle_deg * 100.0f, std::round(target.angle_deg * 100.0f)));
}

void FinalRayAngleMatchesAcRay()
{
    FramePostprocessConfig raw_config;
    raw_config.center_ray_offset_px = 0.0;
    const FrameInferenceResult raw = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 110.0f, 110.0f }, { 20.0f, 10.0f }, 90.0f),
        MakeObb(kBigClass, 0.80f, { 80.0f, 80.0f }, { 16.0f, 8.0f }, 0.0f),
    }, { MakeSegment(cv::Rect(20, 20, 120, 180)) }, raw_config);

    assert(raw.primary_index >= 0);
    const PoseDetection& target = raw.detections[raw.primary_index];
    assert(NearlyEqual(target.angle_deg, AngleBetween(target.arrow_start, target.arrow_end)));
    assert(target.arrow_end.x > target.arrow_start.x);
    assert(NearlyEqual(target.angle_deg, 0.0f));
    assert(NearlyEqual(target.grip_long_angle_deg, 90.0f));
    assert(NearlyEqual(target.obb_center.x, 110.0f));
    assert(NearlyEqual(target.obb_center.y, 110.0f));
}

void AcRayKeepsOriginalCenterBeforeMechanicalGripperFinalization()
{
    FramePostprocessConfig config;
    config.center_ray_offset_px = 10.0;
    const FrameInferenceResult result = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 110.0f, 110.0f }, { 20.0f, 10.0f }, 90.0f),
        MakeObb(kBigClass, 0.80f, { 80.0f, 80.0f }, { 16.0f, 8.0f }, 0.0f),
    }, { MakeSegment(cv::Rect(20, 20, 120, 180)) }, config);

    assert(result.primary_index >= 0);
    const PoseDetection& target = result.detections[result.primary_index];
    assert(NearlyEqual(target.angle_deg, 0.0f));
    assert(NearlyEqual(target.center_x, 110.0f));
    assert(NearlyEqual(target.center_y, 110.0f));
    assert(NearlyEqual(target.obb_center.y, 110.0f));
    assert(NearlyEqual(target.grip_long_angle_deg, 90.0f));
}

void MissingHeadKeepsTargetRejectedWithoutAcFallback()
{
    FramePostprocessConfig config;
    config.center_ray_offset_px = 10.0;
    const FrameInferenceResult result = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 110.0f, 110.0f }, { 20.0f, 10.0f }, 90.0f),
    }, { MakeSegment(cv::Rect(20, 20, 170, 180)) }, config);

    assert(result.detections.size() == 1);
    const PoseDetection& target = result.detections.front();
    assert(!target.can_grab);
    assert(NearlyEqual(target.arrow_end.x, target.obb_center.x));
    assert(NearlyEqual(target.center_y, 110.0f));
}

void AcRayDoesNotUseOldGripObbDirection()
{
    const FrameInferenceResult result = RunPostprocess({
        MakeObb(kGripClass, 0.90f, { 110.0f, 110.0f }, { 20.0f, 10.0f }, 0.0f),
        MakeObb(kSmallClass, 0.80f, { 80.0f, 80.0f }, { 16.0f, 8.0f }, 90.0f),
    }, { MakeSegment(cv::Rect(20, 20, 120, 180)) });

    assert(result.primary_index >= 0);
    const PoseDetection& target = result.detections[result.primary_index];
    assert(NearlyEqual(target.angle_deg, AngleBetween(target.arrow_start, target.arrow_end)));
    assert(target.arrow_end.x > target.arrow_start.x);
    assert(NearlyEqual(target.angle_deg, 0.0f));
}

void AcRayIntersectingHeadRayRejectsConservatively()
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
    BigHeadBelowRightGripMapsToLocalLeft();
    SmallRightMapsToHeadType4();
    SmallLeftMapsToHeadType2();
    HeadOnlyRejects();
    MissingHeadRejects();
    GripMustMatchExactlyOneSegment();
    ExtendedGripTouchingOtherSegmentNoLongerRejectsInPostprocess();
    ExtendedGripOnlyTouchingOtherSegmentBboxStaysGrabbable();
    ExtendedGripThroughMaskHoleStaysGrabbable();
    NeighborMaskInsideMatchedMaskStaysGrabbable();
    ExpandedGripCollisionNoLongerRejectsInPostprocess();
    EmptyOrWrongSizeNeighborMaskDoesNotFallbackToBbox();
    MultipleNeighborMasksDoNotRejectInPostprocess();
    MultipleHeadCandidatesChooseClosestRightAngle();
    EqualHeadCandidatesUseStableTieBreak();
    SegmentWithMultipleLeftRejects();
    MultipleCompleteTargetsChooseBestConfidence();
    NearTieCompleteTargetsChooseStableSegmentOrder();
    FinalPoseOutputIsQuantized();
    FinalRayAngleMatchesAcRay();
    AcRayKeepsOriginalCenterBeforeMechanicalGripperFinalization();
    MissingHeadKeepsTargetRejectedWithoutAcFallback();
    AcRayDoesNotUseOldGripObbDirection();
    AcRayIntersectingHeadRayRejectsConservatively();

    std::cout << "frame_postprocess_smoke passed" << std::endl;
    return 0;
}
