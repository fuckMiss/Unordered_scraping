#pragma once

#include <QGuiApplication>
#include <QMargins>
#include <QRect>
#include <QScreen>
#include <QSize>
#include <QString>
#include <QtGlobal>

inline double g_grasp_responsive_ui_scale = 1.0;
inline double g_grasp_sidebar_compact_scale = 1.0;

inline double GraspBaseUiScale()
{
    if (!QString::fromLocal8Bit(qgetenv("QT_SCALE_FACTOR")).trimmed().isEmpty()) {
        return 1.0;
    }

    const QString override_scale = QString::fromLocal8Bit(qgetenv("TANKEYE_UI_SCALE")).trimmed();
    if (!override_scale.isEmpty()) {
        bool ok = false;
        const double value = override_scale.toDouble(&ok);
        if (ok && value > 0.0) {
            return qBound(0.65, value, 1.20);
        }
    }

    if (QScreen* screen = QGuiApplication::primaryScreen()) {
        const QRect available = screen->availableGeometry();
        const double width_scale = static_cast<double>(available.width()) / 1360.0;
        const double height_scale = static_cast<double>(available.height()) / 820.0;
        double scale = qMin(width_scale, height_scale);
        if (available.width() <= 1366 || available.height() <= 720) {
            scale *= 0.88;
        }
        return qBound(0.65, scale, 1.0);
    }
    return 1.0;
}

inline double GraspUiScale()
{
    return qBound(0.60, GraspBaseUiScale() * g_grasp_responsive_ui_scale, 1.25);
}

inline int GraspS(int value)
{
    return qMax(1, qRound(value * GraspUiScale()));
}

inline QSize GraspSS(int width, int height)
{
    return QSize(GraspS(width), GraspS(height));
}

inline double GraspTopBarScale()
{
    return qBound(0.70, GraspUiScale(), 1.0);
}

inline int GraspTS(int value)
{
    return qMax(1, qRound(value * GraspTopBarScale()));
}

inline QSize GraspTSS(int width, int height)
{
    return QSize(GraspTS(width), GraspTS(height));
}

inline int GraspSC(int value)
{
    return qMax(1, qRound(value * GraspUiScale() * g_grasp_sidebar_compact_scale));
}

inline QMargins GraspSCM(int left, int top, int right, int bottom)
{
    return QMargins(GraspSC(left), GraspSC(top), GraspSC(right), GraspSC(bottom));
}

inline QMargins GraspSM(int left, int top, int right, int bottom)
{
    return QMargins(GraspS(left), GraspS(top), GraspS(right), GraspS(bottom));
}
