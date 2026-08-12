#include "coordinate_transform.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

bool NearlyEqual(float left, float right)
{
    return std::fabs(left - right) < 0.01f;
}

CoordinateTransformConfig MakeScaleConfig()
{
    CoordinateTransformConfig config;
    config.enabled = true;
    config.points = {
        { 0.0, 0.0, 100.0, 200.0 },
        { 10.0, 0.0, 120.0, 200.0 },
        { 0.0, 10.0, 100.0, 230.0 },
        { 10.0, 10.0, 120.0, 230.0 },
    };
    return config;
}

void ValidTransformMapsImageToMachine()
{
    const CoordinateTransformState state = BuildCoordinateTransformState(MakeScaleConfig());
    assert(state.enabled);
    assert(state.valid);
    assert(NearlyEqual(static_cast<float>(state.mean_reprojection_error), 0.0f));

    cv::Point2f machine;
    assert(TransformImagePointToMachine(state, { 5.0f, 5.0f }, &machine));
    assert(NearlyEqual(machine.x, 110.0f));
    assert(NearlyEqual(machine.y, 215.0f));

    cv::Point2f image;
    assert(TransformMachinePointToImage(state, { 110.0f, 215.0f }, &image));
    assert(NearlyEqual(image.x, 5.0f));
    assert(NearlyEqual(image.y, 5.0f));
}

void InvalidTransformDoesNotMapMachineToImage()
{
    CoordinateTransformConfig config;
    config.enabled = true;
    const CoordinateTransformState state = BuildCoordinateTransformState(config);

    cv::Point2f image;
    assert(!TransformMachinePointToImage(state, { 110.0f, 215.0f }, &image));
    assert(!TransformMachinePointToImage(state, { 110.0f, 215.0f }, nullptr));
}

void InvalidPointCountFails()
{
    CoordinateTransformConfig config;
    config.enabled = true;
    config.points = {
        { 0.0, 0.0, 0.0, 0.0 },
        { 1.0, 0.0, 1.0, 0.0 },
        { 0.0, 1.0, 0.0, 1.0 },
    };

    const CoordinateTransformState state = BuildCoordinateTransformState(config);
    assert(state.enabled);
    assert(!state.valid);
}

void ApplyTransformDoesNotInventMachineCoordinatesOnFailure()
{
    CoordinateTransformConfig config;
    config.enabled = true;
    const CoordinateTransformState state = BuildCoordinateTransformState(config);

    FrameInferenceResult result;
    PoseDetection detection;
    detection.center_x = 5.0f;
    detection.center_y = 5.0f;
    detection.machine_x = 999.0f;
    detection.machine_y = 999.0f;
    detection.has_machine_coords = true;
    result.detections.push_back(detection);

    ApplyCoordinateTransform(result, state);
    assert(!result.detections[0].has_machine_coords);
    assert(NearlyEqual(result.detections[0].machine_x, 0.0f));
    assert(NearlyEqual(result.detections[0].machine_y, 0.0f));
}

} // namespace

int main()
{
    ValidTransformMapsImageToMachine();
    InvalidTransformDoesNotMapMachineToImage();
    InvalidPointCountFails();
    ApplyTransformDoesNotInventMachineCoordinatesOnFailure();

    std::cout << "coordinate_transform_test passed" << std::endl;
    return 0;
}
