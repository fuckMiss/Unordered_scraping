#pragma once

#include "frame_result.h"

#include <QObject>

#include <functional>

class RobotController : public QObject
{
public:
    using DoneCallback = std::function<void()>;

    explicit RobotController(QObject* parent = nullptr);

    bool isBusy() const;
    void grabAsync(const PoseDetection& target, DoneCallback on_returned_home);
    void cancel();

private:
    bool busy_ = false;
    int generation_ = 0;
};
