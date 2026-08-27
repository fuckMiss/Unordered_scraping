#pragma once

#include "grasp_main_window.h"

#include <QString>

#include <chrono>

inline double GraspMainWindowMsSince(const std::chrono::steady_clock::time_point& start,
                                     const std::chrono::steady_clock::time_point& end)
{
    return static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()) / 1000.0;
}

inline QString GraspMainWindowDisplayOverlayModeText(DisplayOverlayMode mode)
{
    if (mode == DisplayOverlayMode::AllDebug) {
        return QStringLiteral("全显");
    }
    if (mode == DisplayOverlayMode::None) {
        return QStringLiteral("无显示");
    }
    return QStringLiteral("隐藏");
}

inline QString GraspMainWindowDisplayOverlayModeFileSuffix(DisplayOverlayMode mode)
{
    if (mode == DisplayOverlayMode::AllDebug) {
        return QStringLiteral("all");
    }
    if (mode == DisplayOverlayMode::None) {
        return QStringLiteral("none");
    }
    return QStringLiteral("hidden");
}

inline bool GraspMainWindowIsAllDebugMode(DisplayOverlayMode mode)
{
    return mode == DisplayOverlayMode::AllDebug;
}
