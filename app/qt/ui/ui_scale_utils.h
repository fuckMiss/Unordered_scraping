#pragma once

#include <QGuiApplication>
#include <QMargins>
#include <QRect>
#include <QScreen>
#include <QSize>
#include <QString>
#include <QtGlobal>

inline double CurrentUiScale()
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
        double scale = qMin(available.width() / 1360.0, available.height() / 820.0);
        if (available.width() <= 1366 || available.height() <= 720) {
            scale *= 0.88;
        }
        return qBound(0.65, scale, 1.0);
    }
    return 1.0;
}

inline int ScalePx(int value, double ui_scale)
{
    return qMax(1, qRound(value * ui_scale));
}

inline int ScalePx(int value)
{
    return ScalePx(value, CurrentUiScale());
}

inline QSize ScaleSize(int width, int height, double ui_scale)
{
    return QSize(ScalePx(width, ui_scale), ScalePx(height, ui_scale));
}

inline QSize ScaleSize(int width, int height)
{
    return ScaleSize(width, height, CurrentUiScale());
}

inline QMargins ScaleMargins(int left, int top, int right, int bottom, double ui_scale)
{
    return QMargins(ScalePx(left, ui_scale),
                    ScalePx(top, ui_scale),
                    ScalePx(right, ui_scale),
                    ScalePx(bottom, ui_scale));
}

inline QMargins ScaleMargins(int left, int top, int right, int bottom)
{
    return ScaleMargins(left, top, right, bottom, CurrentUiScale());
}
