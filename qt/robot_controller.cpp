#include "robot_controller.h"

#include <QTimer>

#include <iostream>
#include <utility>

RobotController::RobotController(QObject* parent)
    : QObject(parent)
{
}

bool RobotController::isBusy() const
{
    return busy_;
}

void RobotController::grabAsync(const PoseDetection& target, DoneCallback on_returned_home)
{
    busy_ = true;
    const int request_generation = ++generation_;
    std::cout << "[Robot] mock grab started: x=" << target.center_x
              << ", y=" << target.center_y
              << ", angle=" << target.angle_deg
              << ", class=" << target.class_name
              << std::endl;

    QTimer::singleShot(2000, this, [this, request_generation, on_returned_home = std::move(on_returned_home)]() mutable {
        if (request_generation != generation_) {
            std::cout << "[Robot] mock grab callback ignored because request was cancelled" << std::endl;
            return;
        }

        busy_ = false;
        std::cout << "[Robot] mock robot returned home, firing callback" << std::endl;
        if (on_returned_home) {
            on_returned_home();
        }
    });
}

void RobotController::cancel()
{
    if (busy_) {
        std::cout << "[Robot] mock grab cancelled" << std::endl;
    }
    busy_ = false;
    ++generation_;
}
