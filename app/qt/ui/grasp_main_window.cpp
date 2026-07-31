#include "grasp_main_window.h"

#include "engineering_settings_dialog_controller.h"
#include "engineering_settings_dialog_helpers.h"
#include "engineering_settings_service.h"
#include "admin_auth_helpers.h"
#include "frame_overlay.h"
#include "frame_processing_service.h"
#include "plc_trigger_coordinator.h"
#include "runtime_status_presenter.h"

#include <QApplication>
#include <QClipboard>
#include <QtConcurrent/QtConcurrent>
#include <QCheckBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QScreen>
#include <QSettings>
#include <QTimer>
#include <QPixmap>
#include <QPushButton>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QShowEvent>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTextEdit>
#include <QTextCursor>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <utility>

using namespace cv;
using namespace std;

namespace {

constexpr int kSidebarMinWidth = 96;
constexpr int kSidebarDefaultWidth = 136;
constexpr int kSidebarMaxWidth = 10000;
constexpr double kSidebarWidthRatio = 0.25;
constexpr int kSidebarCompactBaseWidth = 390;
constexpr int kSidebarToggleWidth = 24;
constexpr int kSidebarToggleHeight = 84;
constexpr int kDesignWidth = 1360;
constexpr int kDesignHeight = 820;
constexpr int kDensityBaseWidth = 1280;
constexpr int kDensityBaseHeight = 720;

double g_responsive_ui_scale = 1.0;
double g_sidebar_compact_scale = 1.0;

constexpr int kAdminSaltBytes = 16;

QByteArray RandomSalt()
{
    QByteArray salt;
    salt.resize(kAdminSaltBytes);
    for (int i = 0; i < salt.size(); ++i) {
        salt[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    return salt;
}

QString HashAdminPassword(const QString& password, const QByteArray& salt)
{
    QByteArray payload = salt;
    payload.append(password.toUtf8());
    return QString::fromLatin1(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
}

QString EncodeRememberedPassword(const QString& password)
{
    return QString::fromLatin1(password.toUtf8().toBase64());
}

QString DecodeRememberedPassword(const QString& encoded)
{
    return QString::fromUtf8(QByteArray::fromBase64(encoded.toLatin1()));
}

double MsSince(const std::chrono::steady_clock::time_point& start,
               const std::chrono::steady_clock::time_point& end)
{
    return static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()) / 1000.0;
}

double BaseUiScale()
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
        const double width_scale = static_cast<double>(available.width()) / kDesignWidth;
        const double height_scale = static_cast<double>(available.height()) / kDesignHeight;
        double scale = qMin(width_scale, height_scale);
        if (available.width() <= 1366 || available.height() <= 720) {
            scale *= 0.88;
        }
        return qBound(0.65, scale, 1.0);
    }
    return 1.0;
}

double UiScale()
{
    return qBound(0.60, BaseUiScale() * g_responsive_ui_scale, 1.25);
}

int S(int value)
{
    return qMax(1, qRound(value * UiScale()));
}

QSize SS(int width, int height)
{
    return QSize(S(width), S(height));
}

double TopBarScale()
{
    return qBound(0.70, UiScale(), 1.0);
}

int TS(int value)
{
    return qMax(1, qRound(value * TopBarScale()));
}

QSize TSS(int width, int height)
{
    return QSize(TS(width), TS(height));
}

int SC(int value)
{
    return qMax(1, qRound(value * UiScale() * g_sidebar_compact_scale));
}

QMargins SCM(int left, int top, int right, int bottom)
{
    return QMargins(SC(left), SC(top), SC(right), SC(bottom));
}

QMargins SM(int left, int top, int right, int bottom)
{
    return QMargins(S(left), S(top), S(right), S(bottom));
}

QLabel* CreateSectionTitle(const QString& text)
{
    auto* label = new QLabel(text);
    label->setObjectName("sectionTitle");
    return label;
}

QFrame* CreateMetricCell(const QString& name, QLabel*& value_label, QWidget* parent)
{
    auto* card = new QFrame(parent);
    card->setObjectName("metricCell");

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(SM(10, 8, 10, 8));
    layout->setSpacing(S(4));

    auto* name_label = new QLabel(name, card);
    name_label->setObjectName("metricName");
    value_label = new QLabel(QStringLiteral("--"), card);
    value_label->setObjectName("metricValue");

    layout->addWidget(name_label);
    layout->addWidget(value_label);
    return card;
}

QFrame* CreateKeyValueRow(const QString& name, QLabel*& value_label, QWidget* parent)
{
    auto* row = new QFrame(parent);
    row->setObjectName("keyValueRow");
    row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    row->setMinimumWidth(0);

    auto* layout = new QVBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(SC(3));

    auto* key_cell = new QFrame(row);
    key_cell->setObjectName("keyCell");
    key_cell->setProperty("keyMetricCell", true);
    key_cell->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    key_cell->setMinimumSize(QSize(0, SC(24)));
    key_cell->setMaximumWidth(QWIDGETSIZE_MAX);
    auto* key_layout = new QHBoxLayout(key_cell);
    key_layout->setContentsMargins(SCM(5, 0, 5, 0));
    auto* name_label = new QLabel(name, key_cell);
    name_label->setObjectName("keyLabel");
    name_label->setAlignment(Qt::AlignCenter);
    name_label->setMinimumWidth(0);
    key_layout->addWidget(name_label);

    auto* value_cell = new QFrame(row);
    value_cell->setObjectName("valueCell");
    value_cell->setProperty("valueMetricCell", true);
    value_cell->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    value_cell->setMinimumSize(QSize(0, SC(38)));
    auto* value_layout = new QHBoxLayout(value_cell);
    value_layout->setContentsMargins(SCM(5, 0, 5, 0));

    value_label = new QLabel(QStringLiteral("--"), value_cell);
    value_label->setObjectName("valueLabel");
    value_label->setAlignment(Qt::AlignCenter);
    value_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    value_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    value_label->setMinimumWidth(0);
    value_label->setMinimumHeight(SC(24));
    value_label->setWordWrap(true);
    value_layout->addWidget(value_label, 1);

    layout->addWidget(key_cell);
    layout->addWidget(value_cell);
    return row;
}

QString AdminAuthDialogStyle()
{
    return QString(
               "QDialog { background: #101720; }"
               "QLabel { color: #edf4fb; font-size: %1px; }"
               "QLabel#authHintLabel { color: #a7b9c9; font-size: %2px; }"
               "QLineEdit { background: #f8fafc; color: #10202d; border: 1px solid #6e86a0; border-radius: %3px; padding: %4px %5px; font-size: %1px; min-height: %6px; selection-background-color: #2c7be5; }"
               "QLineEdit[readOnly=\"true\"] { background: #dce6ef; color: #243444; }"
               "QPushButton { background: #1f79db; color: #ffffff; border: 1px solid #5b97e2; border-radius: %3px; padding: %4px %5px; font-size: %1px; font-weight: 600; min-height: %6px; }"
               "QPushButton:hover { background: #2f87e5; }"
               "QPushButton#secondaryAuthButton { background: #2a3440; color: #eef4fb; border: 1px solid #556679; }"
               "QPushButton#secondaryAuthButton:hover { background: #344150; }"
               "QCheckBox { color: #edf4fb; font-size: %2px; spacing: %7px; }"
               "QLabel#authCopyToast { color: #9be7b0; font-size: %2px; font-weight: 600; }"
               "QToolButton#passwordEyeButton { background: transparent; border: none; padding: 0px; }"
               "QToolButton#passwordEyeButton:hover { background: transparent; }")
        .arg(S(15))
        .arg(S(13))
        .arg(S(8))
        .arg(S(8))
        .arg(S(10))
        .arg(S(32))
        .arg(S(8));
}

QLabel* CreateAuthFormLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    label->setMinimumWidth(S(104));
    return label;
}

void ShowCopyToast(QLabel* toast)
{
    if (!toast) {
        return;
    }

    auto* effect = qobject_cast<QGraphicsOpacityEffect*>(toast->graphicsEffect());
    if (!effect) {
        effect = new QGraphicsOpacityEffect(toast);
        toast->setGraphicsEffect(effect);
    }
    effect->setOpacity(1.0);
    toast->setVisible(true);

    auto* animation = new QPropertyAnimation(effect, "opacity", toast);
    animation->setDuration(1200);
    animation->setStartValue(1.0);
    animation->setEndValue(0.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(animation, &QPropertyAnimation::finished, toast, [toast]() {
        toast->setVisible(false);
    });
    QTimer::singleShot(650, toast, [animation]() {
        animation->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

void SetValueText(QLabel* label, const QString& text)
{
    if (!label) {
        return;
    }
    label->setText(text);
    label->setToolTip(text);
}

QLabel* CreateRuntimeBadge(const QString& object_name)
{
    auto* label = new QLabel(QStringLiteral("--"));
    label->setObjectName(object_name);
    label->setAlignment(Qt::AlignCenter);
    return label;
}

QImage MatToQImage(const Mat& image)
{
    if (image.empty()) {
        return QImage();
    }

    if (image.type() == CV_8UC1) {
        return QImage(image.data,
                      image.cols,
                      image.rows,
                      static_cast<int>(image.step),
                      QImage::Format_Grayscale8).copy();
    }

    Mat rgb;
    cvtColor(image, rgb, COLOR_BGR2RGB);
    return QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888).copy();
}

QSize FitImageSize(int source_width, int source_height, const QSize& bounds)
{
    if (source_width <= 0 || source_height <= 0 || bounds.width() <= 0 || bounds.height() <= 0) {
        return QSize(source_width, source_height);
    }

    const double scale = qMin(bounds.width() / static_cast<double>(source_width),
                              bounds.height() / static_cast<double>(source_height));
    return QSize(qMax(1, qRound(source_width * scale)),
                 qMax(1, qRound(source_height * scale)));
}

Point2f ScalePoint(const Point2f& point, double scale_x, double scale_y, int offset_x, int offset_y)
{
    return Point2f(static_cast<float>(point.x * scale_x - offset_x),
                   static_cast<float>(point.y * scale_y - offset_y));
}

Rect ScaleRect(const Rect& rect, double scale_x, double scale_y, int offset_x, int offset_y)
{
    return Rect(qRound(rect.x * scale_x) - offset_x,
                qRound(rect.y * scale_y) - offset_y,
                qRound(rect.width * scale_x),
                qRound(rect.height * scale_y));
}

void ScalePoints(vector<Point2f>& points, double scale_x, double scale_y, int offset_x, int offset_y)
{
    for (Point2f& point : points) {
        point = ScalePoint(point, scale_x, scale_y, offset_x, offset_y);
    }
}

FrameInferenceResult ScaleResultForDisplay(const FrameInferenceResult& result,
                                           double scale_x,
                                           double scale_y,
                                           const Size& scaled_size,
                                           const Size& display_size,
                                           int offset_x,
                                           int offset_y,
                                           bool include_segment_masks)
{
    FrameInferenceResult scaled = result;
    scaled.image_width = display_size.width;
    scaled.image_height = display_size.height;
    scaled.display_scale = qMin(scale_x, scale_y);

    for (PoseDetection& detection : scaled.detections) {
        detection.center_x = static_cast<float>(detection.center_x * scale_x - offset_x);
        detection.center_y = static_cast<float>(detection.center_y * scale_y - offset_y);
        detection.obb_center = ScalePoint(detection.obb_center, scale_x, scale_y, offset_x, offset_y);
        detection.seg_center = ScalePoint(detection.seg_center, scale_x, scale_y, offset_x, offset_y);
        detection.x_point = ScalePoint(detection.x_point, scale_x, scale_y, offset_x, offset_y);
        detection.small_point = ScalePoint(detection.small_point, scale_x, scale_y, offset_x, offset_y);
        detection.small_arrow_start = ScalePoint(detection.small_arrow_start, scale_x, scale_y, offset_x, offset_y);
        detection.small_arrow_end = ScalePoint(detection.small_arrow_end, scale_x, scale_y, offset_x, offset_y);
        detection.arrow_start = ScalePoint(detection.arrow_start, scale_x, scale_y, offset_x, offset_y);
        detection.arrow_end = ScalePoint(detection.arrow_end, scale_x, scale_y, offset_x, offset_y);
        detection.bbox = ScaleRect(detection.bbox, scale_x, scale_y, offset_x, offset_y);
        ScalePoints(detection.corners, scale_x, scale_y, offset_x, offset_y);
        ScalePoints(detection.extended_corners, scale_x, scale_y, offset_x, offset_y);
    }

    for (ObbRegion& region : scaled.raw_obb_regions) {
        region.center = ScalePoint(region.center, scale_x, scale_y, offset_x, offset_y);
        region.bbox = ScaleRect(region.bbox, scale_x, scale_y, offset_x, offset_y);
        ScalePoints(region.corners, scale_x, scale_y, offset_x, offset_y);
    }

    for (SegRegion& segment : scaled.segments) {
        segment.bbox = ScaleRect(segment.bbox, scale_x, scale_y, offset_x, offset_y);
        segment.center = ScalePoint(segment.center, scale_x, scale_y, offset_x, offset_y);
        ScalePoints(segment.contour, scale_x, scale_y, offset_x, offset_y);
        ScalePoints(segment.min_rect_corners, scale_x, scale_y, offset_x, offset_y);
        if (include_segment_masks && !segment.mask.empty()) {
            Mat resized_mask;
            resize(segment.mask, resized_mask, scaled_size, 0.0, 0.0, INTER_NEAREST);
            const Rect crop_rect(offset_x, offset_y, display_size.width, display_size.height);
            segment.mask = resized_mask(crop_rect).clone();
        } else {
            segment.mask.release();
        }
    }

    return scaled;
}

QString FormatNumber(float value)
{
    return QString::number(value, 'f', 1);
}

QString FormatMachineCoordinate(const PoseDetection& detection, bool x_axis)
{
    if (!detection.has_machine_coords) {
        return QStringLiteral("--");
    }
    return FormatNumber(x_axis ? detection.machine_x : detection.machine_y);
}

QString FormatPrimaryCoordinate(const PoseDetection& detection, bool x_axis)
{
    if (detection.has_machine_coords) {
        return FormatNumber(x_axis ? detection.machine_x : detection.machine_y);
    }
    return FormatNumber(x_axis ? detection.center_x : detection.center_y);
}

QString FormatPlcWriteResult(const PlcWriteResult& result, const PlcRegisterMap& registers)
{
    const QString machine_text = result.has_machine_coords
        ? QStringLiteral("D%1前后轴值：%2\nD%3左右轴值：%4")
              .arg(registers.front_back)
              .arg(FormatNumber(result.x))
              .arg(registers.left_right)
              .arg(FormatNumber(result.y))
        : QStringLiteral("机械X：未启用\n机械Y：未启用\n");
    return QStringLiteral("图像X：%1\n图像Y：%2\n%3D%4 前后：%5\nD%6 左右：%7\nD%8 角度：%9\nD%10 状态：%11\nD%12 类型：%13")
        .arg(FormatNumber(result.image_x))
        .arg(FormatNumber(result.image_y))
        .arg(machine_text)
        .arg(registers.front_back)
        .arg(FormatNumber(result.x))
        .arg(registers.left_right)
        .arg(FormatNumber(result.y))
        .arg(registers.angle)
        .arg(FormatNumber(result.angle))
        .arg(registers.pick_status)
        .arg(FormatNumber(result.pick_status))
        .arg(registers.head_type)
        .arg(FormatNumber(result.head_type));
}

QString FormatLimitRange(double lower, double upper)
{
    return QStringLiteral("%1 ~ %2")
        .arg(QString::number(lower, 'f', 1),
             QString::number(upper, 'f', 1));
}

QString DefaultLogoPath()
{
    const QString relative_path = QStringLiteral("app/qt/assets/app_logo_cutout.png");
    const QStringList candidates = {
        relative_path,
        QDir(QCoreApplication::applicationDirPath()).filePath(relative_path),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../") + relative_path),
    };

    for (const QString& candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return relative_path;
}

qreal IconDevicePixelRatio()
{
    if (QScreen* screen = QGuiApplication::primaryScreen()) {
        return qMax<qreal>(1.0, screen->devicePixelRatio());
    }
    return 1.0;
}

QPixmap CreateIconPixmap(const QSize& logical_size)
{
    const qreal dpr = IconDevicePixelRatio();
    QPixmap pixmap(qMax(1, qCeil(logical_size.width() * dpr)),
                   qMax(1, qCeil(logical_size.height() * dpr)));
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    return pixmap;
}

QIcon CreateAvatarIcon(const QSize& size, const QColor& color)
{
    QPixmap pixmap = CreateIconPixmap(size);

    QPainter painter(&pixmap);
    painter.scale(IconDevicePixelRatio(), IconDevicePixelRatio());
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    const qreal w = size.width();
    const qreal h = size.height();
    painter.drawEllipse(QRectF(w * 0.31, h * 0.12, w * 0.38, h * 0.38));

    QPainterPath body_path;
    body_path.moveTo(w * 0.19, h * 0.84);
    body_path.cubicTo(w * 0.20, h * 0.56, w * 0.80, h * 0.56, w * 0.81, h * 0.84);
    painter.drawPath(body_path);
    return QIcon(pixmap);
}

QIcon CreateGearIcon(const QSize& size, const QColor& color)
{
    QPixmap pixmap = CreateIconPixmap(size);

    QPainter painter(&pixmap);
    painter.scale(IconDevicePixelRatio(), IconDevicePixelRatio());
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate(size.width() / 2.0, size.height() / 2.0);

    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    for (int i = 0; i < 8; ++i) {
        painter.save();
        painter.rotate(i * 45.0);
        painter.drawRoundedRect(QRectF(-1.35, -size.height() * 0.46, 2.7, size.height() * 0.20), 1.1, 1.1);
        painter.restore();
    }

    painter.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QRectF(-size.width() * 0.26, -size.height() * 0.26, size.width() * 0.52, size.height() * 0.52));
    painter.drawEllipse(QRectF(-size.width() * 0.10, -size.height() * 0.10, size.width() * 0.20, size.height() * 0.20));
    return QIcon(pixmap);
}

QIcon CreateMinimizeIcon(const QSize& size, const QColor& color)
{
    QPixmap pixmap = CreateIconPixmap(size);

    QPainter painter(&pixmap);
    painter.scale(IconDevicePixelRatio(), IconDevicePixelRatio());
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color, 1.9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(QPointF(size.width() * 0.24, size.height() * 0.68),
                     QPointF(size.width() * 0.76, size.height() * 0.68));
    return QIcon(pixmap);
}

QIcon CreateMaximizeIcon(const QSize& size, const QColor& color)
{
    QPixmap pixmap = CreateIconPixmap(size);

    QPainter painter(&pixmap);
    painter.scale(IconDevicePixelRatio(), IconDevicePixelRatio());
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(size.width() * 0.24,
                                   size.height() * 0.24,
                                   size.width() * 0.52,
                                   size.height() * 0.52),
                            1.8,
                            1.8);
    return QIcon(pixmap);
}

QIcon CreateRestoreIcon(const QSize& size, const QColor& color)
{
    QPixmap pixmap = CreateIconPixmap(size);

    QPainter painter(&pixmap);
    painter.scale(IconDevicePixelRatio(), IconDevicePixelRatio());
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(size.width() * 0.34,
                                   size.height() * 0.20,
                                   size.width() * 0.38,
                                   size.height() * 0.38),
                            1.6,
                            1.6);
    painter.drawRoundedRect(QRectF(size.width() * 0.22,
                                   size.height() * 0.34,
                                   size.width() * 0.38,
                                   size.height() * 0.38),
                            1.6,
                            1.6);
    return QIcon(pixmap);
}

QIcon CreateCloseIcon(const QSize& size, const QColor& color)
{
    QPixmap pixmap = CreateIconPixmap(size);

    QPainter painter(&pixmap);
    painter.scale(IconDevicePixelRatio(), IconDevicePixelRatio());
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color, 1.9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(QPointF(size.width() * 0.28, size.height() * 0.28),
                     QPointF(size.width() * 0.72, size.height() * 0.72));
    painter.drawLine(QPointF(size.width() * 0.72, size.height() * 0.28),
                     QPointF(size.width() * 0.28, size.height() * 0.72));
    return QIcon(pixmap);
}

int SidebarMinWidth()
{
    return S(kSidebarMinWidth);
}

int SidebarDefaultWidth()
{
    return S(kSidebarDefaultWidth);
}

int SidebarMaxWidth()
{
    return S(kSidebarMaxWidth);
}

int ClampSidebarWidth(int width)
{
    return qBound(SidebarMinWidth(), width, SidebarMaxWidth());
}

void ClearLayoutItems(QLayout* layout)
{
    if (!layout) {
        return;
    }

    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            delete widget;
        }
        if (QLayout* child_layout = item->layout()) {
            ClearLayoutItems(child_layout);
            delete child_layout;
        }
        delete item;
    }
}

Qt::Edges HitTestResizeEdges(const QWidget* window, const QPoint& global_pos, int margin)
{
    Qt::Edges edges;
    const QRect frame = window->frameGeometry();
    if (global_pos.x() - frame.left() <= margin) {
        edges |= Qt::LeftEdge;
    }
    if (frame.right() - global_pos.x() <= margin) {
        edges |= Qt::RightEdge;
    }
    if (global_pos.y() - frame.top() <= margin) {
        edges |= Qt::TopEdge;
    }
    if (frame.bottom() - global_pos.y() <= margin) {
        edges |= Qt::BottomEdge;
    }
    return edges;
}

Qt::CursorShape CursorShapeForEdges(Qt::Edges edges)
{
    const bool left = edges.testFlag(Qt::LeftEdge);
    const bool right = edges.testFlag(Qt::RightEdge);
    const bool top = edges.testFlag(Qt::TopEdge);
    const bool bottom = edges.testFlag(Qt::BottomEdge);

    if ((left && top) || (right && bottom)) {
        return Qt::SizeFDiagCursor;
    }
    if ((right && top) || (left && bottom)) {
        return Qt::SizeBDiagCursor;
    }
    if (left || right) {
        return Qt::SizeHorCursor;
    }
    if (top || bottom) {
        return Qt::SizeVerCursor;
    }
    return Qt::ArrowCursor;
}

QRect ResizeGeometryFromDrag(const QRect& start_geometry,
                             const QPoint& start_pos,
                             const QPoint& current_pos,
                             Qt::Edges edges,
                             const QSize& minimum_size)
{
    QRect next_geometry = start_geometry;
    const QPoint delta = current_pos - start_pos;

    if (edges.testFlag(Qt::LeftEdge)) {
        const int max_left = start_geometry.right() - minimum_size.width() + 1;
        next_geometry.setLeft(std::min(start_geometry.left() + delta.x(), max_left));
    }
    if (edges.testFlag(Qt::RightEdge)) {
        const int min_right = start_geometry.left() + minimum_size.width() - 1;
        next_geometry.setRight(std::max(start_geometry.right() + delta.x(), min_right));
    }
    if (edges.testFlag(Qt::TopEdge)) {
        const int max_top = start_geometry.bottom() - minimum_size.height() + 1;
        next_geometry.setTop(std::min(start_geometry.top() + delta.y(), max_top));
    }
    if (edges.testFlag(Qt::BottomEdge)) {
        const int min_bottom = start_geometry.top() + minimum_size.height() - 1;
        next_geometry.setBottom(std::max(start_geometry.bottom() + delta.y(), min_bottom));
    }

    return next_geometry;
}

}  // namespace

GraspMainWindow::GraspMainWindow(QWidget* parent)
    : GraspMainWindow(AppConfigService::Load(), parent)
{
}

GraspMainWindow::GraspMainWindow(const AppConfig& app_config, QWidget* parent)
    : QMainWindow(parent)
    , app_config_(app_config)
    , robot_controller_(app_config_.plc, this)
{
    engineering_settings_dialog_controller_ =
        std::make_unique<EngineeringSettingsDialogController>(*this);
    plc_poll_timer_ = new QTimer(this);
    plc_poll_timer_->setInterval(50);
    loadLimitSettings();
    loadCameraSettings();
    loadModelThresholdSettings();
    loadAngleCalibrationSettings();
    loadObbPostprocessSettings();
    loadAxisMappingSettings();
    loadAxisCompensationSettings();
    loadCoordinateTransformSettings();
    show_all_detections_ = false;
    setupUi();
    applyStyles();
    bindActions();
    refreshResultPresentation(false);
    setEmptyPreviewMessage(QStringLiteral("请选择图片或打开相机"));
    updateStatusMessage(QStringLiteral("界面已就绪，可在工程设置中加载模型。"));
}

GraspMainWindow::~GraspMainWindow()
{
    workflow_.stopCamera();
    waitForBackgroundJobs();
}

void GraspMainWindow::setInitialModelPaths(const QString& obb_model_path, const QString& seg_model_path)
{
    obb_model_path_ = obb_model_path;
    seg_model_path_ = seg_model_path;
}

void GraspMainWindow::setShowAllDetections(bool show_all_detections)
{
    show_all_detections_ = show_all_detections;
    if (display_mode_button_) {
        display_mode_button_->setText(show_all_detections_ ? QStringLiteral("全显") : QStringLiteral("隐藏"));
    }
    renderCurrentFrame();
    refreshTargetTable();
}

bool GraspMainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == central_panel_) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* mouse_event = static_cast<QMouseEvent*>(event);
            if (mouse_event->button() == Qt::LeftButton && !isMaximized()) {
                const Qt::Edges edges = HitTestResizeEdges(this, mouse_event->globalPos(), 8);
                if (edges != Qt::Edges()) {
                    resizing_frame_ = true;
                    active_resize_edges_ = edges;
                    resize_start_geometry_ = geometry();
                    resize_start_global_pos_ = mouse_event->globalPos();
                    central_panel_->setCursor(CursorShapeForEdges(edges));
                    return true;
                }
            }
            break;
        }
        case QEvent::MouseMove: {
            auto* mouse_event = static_cast<QMouseEvent*>(event);
            if (resizing_frame_) {
                setGeometry(ResizeGeometryFromDrag(
                    resize_start_geometry_, resize_start_global_pos_, mouse_event->globalPos(), active_resize_edges_, minimumSize()));
                return true;
            }

            if (!isMaximized()) {
                const Qt::Edges edges = HitTestResizeEdges(this, mouse_event->globalPos(), 8);
                if (edges != Qt::Edges()) {
                    central_panel_->setCursor(CursorShapeForEdges(edges));
                } else {
                    central_panel_->unsetCursor();
                }
            } else {
                central_panel_->unsetCursor();
            }
            break;
        }
        case QEvent::MouseButtonRelease: {
            auto* mouse_event = static_cast<QMouseEvent*>(event);
            if (mouse_event->button() == Qt::LeftButton && resizing_frame_) {
                resizing_frame_ = false;
                active_resize_edges_ = Qt::Edges();
                if (!isMaximized()) {
                    const Qt::Edges edges = HitTestResizeEdges(this, mouse_event->globalPos(), 8);
                    if (edges != Qt::Edges()) {
                        central_panel_->setCursor(CursorShapeForEdges(edges));
                    } else {
                        central_panel_->unsetCursor();
                    }
                } else {
                    central_panel_->unsetCursor();
                }
                return true;
            }
            break;
        }
        case QEvent::Leave:
            if (!resizing_frame_) {
                central_panel_->unsetCursor();
            }
            break;
        default:
            break;
        }
    }

    if (watched == top_bar_) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* mouse_event = static_cast<QMouseEvent*>(event);
            if (mouse_event->button() == Qt::LeftButton) {
                top_bar_press_active_ = true;
                top_bar_dragging_ = !isMaximized();
                top_bar_press_global_pos_ = mouse_event->globalPos();
                top_bar_press_ratio_ = top_bar_->width() > 0
                                           ? qBound(0.1,
                                                    static_cast<qreal>(mouse_event->pos().x()) / static_cast<qreal>(top_bar_->width()),
                                                    0.9)
                                           : 0.5;
                top_bar_drag_offset_ = mouse_event->globalPos() - frameGeometry().topLeft();
                top_bar_->setCursor(Qt::ClosedHandCursor);
                return true;
            }
            break;
        }
        case QEvent::MouseMove: {
            auto* mouse_event = static_cast<QMouseEvent*>(event);
            if (top_bar_press_active_ && isMaximized() && (mouse_event->buttons() & Qt::LeftButton)) {
                if ((mouse_event->globalPos() - top_bar_press_global_pos_).manhattanLength() >= QApplication::startDragDistance()) {
                    showNormal();
                    syncWindowControlButtons();

                    const int restored_width = width();
                    const int anchor_x = static_cast<int>(restored_width * top_bar_press_ratio_);
                    const QPoint restored_top_left(mouse_event->globalPos().x() - anchor_x,
                                                   mouse_event->globalPos().y() - top_bar_->height() / 2);
                    move(restored_top_left);
                    top_bar_dragging_ = true;
                    top_bar_drag_offset_ = mouse_event->globalPos() - frameGeometry().topLeft();
                }
                return true;
            }

            if (top_bar_dragging_ && (mouse_event->buttons() & Qt::LeftButton)) {
                move(mouse_event->globalPos() - top_bar_drag_offset_);
                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease: {
            auto* mouse_event = static_cast<QMouseEvent*>(event);
            if (mouse_event->button() == Qt::LeftButton) {
                top_bar_press_active_ = false;
                top_bar_dragging_ = false;
                top_bar_->setCursor(Qt::OpenHandCursor);
                return true;
            }
            break;
        }
        case QEvent::MouseButtonDblClick: {
            auto* mouse_event = static_cast<QMouseEvent*>(event);
            if (mouse_event->button() == Qt::LeftButton) {
                if (isMaximized()) {
                    showNormal();
                } else {
                    showMaximized();
                }
                top_bar_press_active_ = false;
                top_bar_dragging_ = false;
                top_bar_->setCursor(Qt::OpenHandCursor);
                syncWindowControlButtons();
                return true;
            }
            break;
        }
        default:
            break;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void GraspMainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    applyResponsiveLayout();
    repositionSidePanelToggle();
    renderCurrentFrame();
}

void GraspMainWindow::closeEvent(QCloseEvent* event)
{
    workflow_.stopCamera();

    if (model_load_watcher_ && !model_load_watcher_->isFinished()) {
        close_after_model_load_ = true;
        hide();
        event->ignore();
        return;
    }

    waitForBackgroundJobs();
    QMainWindow::closeEvent(event);
}

void GraspMainWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);

    if (event->type() == QEvent::WindowStateChange) {
        applyResponsiveLayout(true);
        syncWindowControlButtons();
    }
}

void GraspMainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    applyResponsiveLayout(true);
    syncWindowControlButtons();
    repositionSidePanelToggle();
    logHighDpiMetrics(QStringLiteral("show"));

    if (!initial_models_attempted_) {
        initial_models_attempted_ = true;
        if (!obb_model_path_.trimmed().isEmpty() && !seg_model_path_.trimmed().isEmpty()) {
            QTimer::singleShot(0, this, [this]() {
                loadModelsFromPathsAsync(obb_model_path_, seg_model_path_, false, true);
            });
        }
    }
}

void GraspMainWindow::setupUi()
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    resize(SS(1360, 820));
    setMinimumSize(SS(960, 600));
    setWindowTitle(QStringLiteral("截止阀抓取上料系统"));
    setWindowIcon(QIcon(DefaultLogoPath()));

    central_panel_ = new QWidget(this);
    setCentralWidget(central_panel_);
    central_panel_->setMouseTracking(true);
    central_panel_->installEventFilter(this);

    auto* root_layout = new QVBoxLayout(central_panel_);
    root_layout->setContentsMargins(SM(8, 8, 8, 10));
    root_layout->setSpacing(S(8));

    buildTopBar(root_layout);

    main_splitter_ = new QSplitter(Qt::Horizontal, this);
    main_splitter_->setChildrenCollapsible(false);
    main_splitter_->setHandleWidth(S(8));
    root_layout->addWidget(main_splitter_, 1);

    buildDisplayPanel(main_splitter_);
    buildSidePanel(main_splitter_);
    main_splitter_->setStretchFactor(0, 75);
    main_splitter_->setStretchFactor(1, 25);

    sidebar_toggle_button_ = new QToolButton(central_panel_);
    sidebar_toggle_button_->setObjectName("sideRailToggleButton");
    sidebar_toggle_button_->setCursor(Qt::PointingHandCursor);
    sidebar_toggle_button_->setFixedSize(SS(kSidebarToggleWidth, kSidebarToggleHeight));
    sidebar_toggle_button_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    sidebar_toggle_button_->raise();

    expanded_sidebar_width_ = responsiveSidebarWidth();
    side_panel_expanded_ = true;
    syncSidePanelToggleButton();
    syncMainSplitterRatio();
    refreshSidebarCompactMetrics();
    applyStyles();
    refreshActionButtonMetrics();
    refreshAdminModeUi();
    repositionSidePanelToggle();
    applyResponsiveLayout(true);
}

void GraspMainWindow::buildTopBar(QVBoxLayout* root_layout)
{
    top_bar_ = new QFrame(this);
    top_bar_->setObjectName("topBar");
    top_bar_->setFixedHeight(S(56));
    top_bar_->setCursor(Qt::OpenHandCursor);
    top_bar_->installEventFilter(this);

    auto* top_layout = new QGridLayout(top_bar_);
    top_layout->setContentsMargins(SM(14, 6, 14, 6));
    top_layout->setHorizontalSpacing(S(10));
    top_layout->setVerticalSpacing(0);

    auto* left_widget = new QWidget(top_bar_);
    auto* left_layout = new QHBoxLayout(left_widget);
    left_layout->setContentsMargins(0, 0, 0, 0);
    left_layout->setSpacing(S(10));
    left_widget->setAttribute(Qt::WA_TransparentForMouseEvents);

    logo_label_ = new QLabel(top_bar_);
    logo_label_->setObjectName("logoLabel");
    logo_label_->setFixedSize(SS(50, 42));
    logo_label_->setAlignment(Qt::AlignCenter);
    const QPixmap logo_pixmap(DefaultLogoPath());
    if (!logo_pixmap.isNull()) {
        logo_label_->setPixmap(logo_pixmap.scaled(SS(42, 34), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        logo_label_->setText(QStringLiteral("L"));
    }

    brand_label_ = new QLabel(QStringLiteral("TankEye-Iris"), top_bar_);
    brand_label_->setObjectName("brandLabel");
    brand_label_->setMinimumWidth(0);
    brand_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    left_layout->addWidget(logo_label_);
    left_layout->addWidget(brand_label_);

    title_label_ = new QLabel(QStringLiteral("截止阀抓取上料系统"), top_bar_);
    title_label_->setObjectName("titleLabel");
    title_label_->setAlignment(Qt::AlignCenter);
    title_label_->setMinimumWidth(0);
    title_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    title_label_->setAttribute(Qt::WA_TransparentForMouseEvents);

    top_right_controls_ = new QWidget(top_bar_);
    top_right_controls_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    auto* right_layout = new QHBoxLayout(top_right_controls_);
    right_layout->setContentsMargins(0, 0, 0, 0);
    right_layout->setSpacing(S(6));
    right_layout->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    const QSize icon_size = TSS(18, 18);
    const QSize button_size = TSS(34, 34);
    const QColor icon_color(QStringLiteral("#f0e9e1"));
    const auto init_button = [icon_size, button_size](QToolButton* button,
                                                      const QString& tooltip,
                                                      const char* property_name) {
        button->setCursor(Qt::PointingHandCursor);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setFixedSize(button_size);
        button->setIconSize(icon_size);
        button->setToolTip(tooltip);
        button->setProperty(property_name, true);
    };

    user_button_ = new QToolButton(top_bar_);
    init_button(user_button_, QStringLiteral("用户"), "topBarButton");
    user_button_->setIcon(CreateAvatarIcon(icon_size, icon_color));

    settings_icon_button_ = new QToolButton(top_bar_);
    init_button(settings_icon_button_, QStringLiteral("工程设置"), "topBarButton");
    settings_icon_button_->setIcon(CreateGearIcon(icon_size, icon_color));

    minimize_window_button_ = new QToolButton(top_bar_);
    init_button(minimize_window_button_, QStringLiteral("最小化"), "windowButton");
    minimize_window_button_->setIcon(CreateMinimizeIcon(icon_size, icon_color));

    maximize_window_button_ = new QToolButton(top_bar_);
    init_button(maximize_window_button_, QStringLiteral("最大化"), "windowButton");

    close_window_button_ = new QToolButton(top_bar_);
    init_button(close_window_button_, QStringLiteral("关闭"), "closeWindowButton");
    close_window_button_->setIcon(CreateCloseIcon(icon_size, icon_color));

    right_layout->addWidget(user_button_);
    right_layout->addWidget(settings_icon_button_);
    right_layout->addSpacing(S(2));
    right_layout->addWidget(minimize_window_button_);
    right_layout->addWidget(maximize_window_button_);
    right_layout->addWidget(close_window_button_);

    top_layout->addWidget(title_label_, 0, 0, 1, 3, Qt::AlignCenter);
    top_layout->addWidget(left_widget, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
    top_layout->addWidget(top_right_controls_, 0, 2, Qt::AlignRight | Qt::AlignVCenter);
    top_layout->setColumnStretch(0, 1);
    top_layout->setColumnStretch(1, 1);
    top_layout->setColumnStretch(2, 0);
    top_layout->setColumnMinimumWidth(2, top_right_controls_->sizeHint().width());

    root_layout->addWidget(top_bar_, 0);
    syncWindowControlButtons();
}

void GraspMainWindow::buildDisplayPanel(QSplitter* splitter)
{
    auto* display_panel = new QFrame(this);
    display_panel->setObjectName("displayPanel");
    display_panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* display_layout = new QVBoxLayout(display_panel);
    display_layout->setContentsMargins(SM(16, 12, 16, 16));
    display_layout->setSpacing(S(12));

    buildRuntimeStrip(display_layout);

    image_label_ = new QLabel(this);
    image_label_->setMinimumSize(SS(320, 220));
    image_label_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    image_label_->setAlignment(Qt::AlignCenter);
    image_label_->setObjectName("imageViewport");
    display_layout->addWidget(image_label_, 1);

    splitter->addWidget(display_panel);
}

void GraspMainWindow::buildRuntimeStrip(QVBoxLayout* display_layout)
{
    auto* runtime_bar = new QFrame(this);
    runtime_bar->setObjectName("runtimeBar");
    auto* runtime_layout = new QHBoxLayout(runtime_bar);
    runtime_layout->setContentsMargins(SM(10, 8, 10, 8));
    runtime_layout->setSpacing(S(8));

    runtime_mode_label_ = CreateRuntimeBadge("runtimeBadge");
    runtime_state_label_ = CreateRuntimeBadge("runtimeStateBadge");
    runtime_obb_count_label_ = CreateRuntimeBadge("runtimeBadge");
    runtime_seg_count_label_ = CreateRuntimeBadge("runtimeBadge");
    runtime_stage_time_label_ = CreateRuntimeBadge("runtimeBadge");
    runtime_total_time_label_ = CreateRuntimeBadge("runtimeBadge");

    runtime_layout->addWidget(runtime_state_label_, 0);
    runtime_layout->addWidget(runtime_mode_label_, 0);
    runtime_layout->addWidget(runtime_obb_count_label_, 0);
    runtime_layout->addWidget(runtime_seg_count_label_, 0);
    runtime_layout->addWidget(runtime_stage_time_label_, 0);
    runtime_layout->addWidget(runtime_total_time_label_, 0);
    runtime_layout->addStretch();

    display_layout->addWidget(runtime_bar, 0);
}

void GraspMainWindow::buildSidePanel(QSplitter* splitter)
{
    side_panel_ = new QFrame(this);
    side_panel_->setObjectName("sidePanel");
    side_panel_->setMinimumWidth(SidebarMinWidth());
    side_panel_->setMaximumWidth(SidebarMaxWidth());
    side_panel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);

    auto* side_layout = new QVBoxLayout(side_panel_);
    side_layout->setContentsMargins(SCM(6, 6, 6, 6));
    side_layout->setSpacing(0);

    side_pages_ = new QStackedWidget(side_panel_);

    detail_page_ = new QWidget(side_pages_);
    auto* detail_layout = new QVBoxLayout(detail_page_);
    detail_layout->setContentsMargins(0, 0, 0, 0);
    detail_layout->setSpacing(SC(8));
    buildResultSection(detail_layout);
    buildStatusSection(detail_layout);
    buildActionSection(detail_layout);
    buildFunctionSection(detail_layout);
    side_pages_->addWidget(detail_page_);

    target_list_page_ = new QWidget(side_pages_);
    auto* target_list_layout = new QVBoxLayout(target_list_page_);
    target_list_layout->setContentsMargins(0, 0, 0, 0);
    target_list_layout->setSpacing(S(6));
    buildTargetListSection(target_list_layout);
    side_pages_->addWidget(target_list_page_);

    side_pages_->setCurrentWidget(detail_page_);
    side_layout->addWidget(side_pages_);

    splitter->addWidget(side_panel_);
}

void GraspMainWindow::buildResultSection(QVBoxLayout* side_layout)
{
    const PlcRegisterMap registers = robot_controller_.plcRegisterMap();
    side_layout->addWidget(CreateSectionTitle(QStringLiteral("当前目标参数")));

    auto* overview_card = new QFrame(this);
    overview_card->setObjectName("heroInfoCard");
    overview_card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* overview_layout = new QVBoxLayout(overview_card);
    overview_layout->setContentsMargins(SCM(6, 6, 6, 6));
    overview_layout->setSpacing(SC(6));
    overview_layout->addWidget(CreateGroupCaption(QStringLiteral("当前主目标"), overview_card));
    overview_layout->addWidget(CreateKeyValueRow(QStringLiteral("X坐标"), machine_x_value_label_, overview_card));
    overview_layout->addWidget(CreateKeyValueRow(QStringLiteral("Y坐标"), machine_y_value_label_, overview_card));
    overview_layout->addWidget(CreateKeyValueRow(QStringLiteral("角度"), angle_value_label_, overview_card));
    overview_layout->addWidget(CreateKeyValueRow(QStringLiteral("D%1").arg(registers.pick_status),
                                                pick_status_value_label_,
                                                overview_card));
    overview_layout->addWidget(CreateKeyValueRow(QStringLiteral("大小头"),
                                                head_type_value_label_,
                                                overview_card));

    QList<QWidget*> metric_widgets;
    while (overview_layout->count() > 1) {
        QLayoutItem* item = overview_layout->takeAt(1);
        if (QWidget* widget = item->widget()) {
            metric_widgets.push_back(widget);
        }
        delete item;
    }
    auto* metrics_layout = new QGridLayout();
    metrics_layout->setContentsMargins(0, 0, 0, 0);
    metrics_layout->setHorizontalSpacing(SC(6));
    metrics_layout->setVerticalSpacing(SC(6));
    for (int i = 0; i < metric_widgets.size(); ++i) {
        if (i == 4) {
            metrics_layout->addWidget(metric_widgets[i], 2, 0, 1, 2);
        } else {
            metrics_layout->addWidget(metric_widgets[i], i / 2, i % 2);
        }
    }
    metrics_layout->setColumnStretch(0, 1);
    metrics_layout->setColumnStretch(1, 1);
    overview_layout->addLayout(metrics_layout);

    side_layout->addWidget(overview_card);
}

void GraspMainWindow::buildStatusSection(QVBoxLayout* side_layout)
{
    side_layout->addWidget(CreateSectionTitle(QStringLiteral("系统状态")));

    auto* status_card = new QFrame(this);
    status_card->setObjectName("infoCard");
    status_card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* status_layout = new QVBoxLayout(status_card);
    status_layout->setContentsMargins(SCM(6, 6, 6, 6));
    status_layout->setSpacing(SC(4));

    camera_status_dot_ = new QLabel(this);
    camera_status_value_ = new QLabel(this);
    obb_status_dot_ = new QLabel(this);
    obb_status_value_ = new QLabel(this);
    seg_status_dot_ = new QLabel(this);
    seg_status_value_ = new QLabel(this);

    const auto add_status_item = [this, status_layout](const QString& name, QLabel* dot, QLabel* value) {
        auto* container = new QFrame(this);
        container->setObjectName("statusStackItem");
        auto* layout = new QHBoxLayout(container);
        layout->setContentsMargins(SCM(5, 3, 5, 3));
        layout->setSpacing(SC(4));
        auto* label = new QLabel(name, container);
        label->setObjectName("statusStackName");
        label->setMinimumWidth(0);
        layout->addWidget(label);
        layout->addWidget(dot);
        value->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        layout->addWidget(value, 1);
        status_layout->addWidget(container);
    };

    add_status_item(QStringLiteral("相机"), camera_status_dot_, camera_status_value_);
    add_status_item(QStringLiteral("模型"), obb_status_dot_, obb_status_value_);
    side_layout->addWidget(status_card);
}

void GraspMainWindow::buildActionSection(QVBoxLayout* side_layout)
{
    side_layout->addWidget(CreateSectionTitle(QStringLiteral("作业控制")));

    image_path_label_ = new QLabel(QStringLiteral("等待图像输入"), this);
    image_path_label_->setObjectName("pathHintLabel");
    image_path_label_->setWordWrap(true);

    load_image_button_ = new QPushButton(QStringLiteral("加载图片"), this);
    open_camera_button_ = new QPushButton(QStringLiteral("打开相机"), this);
    start_button_ = new QPushButton(QStringLiteral("开始检测"), this);
    plc_link_button_ = new QPushButton(QStringLiteral("抓取"), this);
    plc_test_button_ = new QPushButton(QStringLiteral("测试"), this);
    stop_button_ = new QPushButton(QStringLiteral("关闭相机"), this);
    display_mode_button_ = new QPushButton(QStringLiteral("全显"), this);
    target_list_button_ = new QPushButton(QStringLiteral("目标列表"), this);
    engineering_button_ = new QPushButton(QStringLiteral("工程设置"), this);
    runtime_log_button_ = new QPushButton(QStringLiteral("运行日志"), this);

    const QList<QPushButton*> action_buttons = {
        load_image_button_, open_camera_button_, start_button_, plc_link_button_,
        plc_test_button_, stop_button_, display_mode_button_, target_list_button_, engineering_button_, runtime_log_button_
    };
    for (QPushButton* button : action_buttons) {
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setMinimumWidth(0);
    }

    const int primary_button_height = S(38);
    const int secondary_button_height = S(34);
    load_image_button_->setMinimumHeight(primary_button_height);
    open_camera_button_->setMinimumHeight(primary_button_height);
    start_button_->setMinimumHeight(primary_button_height);
    plc_link_button_->setMinimumHeight(primary_button_height);
    plc_test_button_->setMinimumHeight(primary_button_height);
    stop_button_->setMinimumHeight(primary_button_height);
    display_mode_button_->setMinimumHeight(secondary_button_height);
    target_list_button_->setMinimumHeight(secondary_button_height);
    engineering_button_->setMinimumHeight(secondary_button_height);
    runtime_log_button_->setMinimumHeight(secondary_button_height);
    display_mode_button_->setObjectName("secondaryActionButton");
    target_list_button_->setObjectName("secondaryActionButton");
    engineering_button_->setObjectName("secondaryActionButton");
    runtime_log_button_->setObjectName("secondaryActionButton");
    plc_link_button_->setObjectName("primaryActionButton");
    stop_button_->setObjectName("stopActionButton");

    auto* input_group = new QFrame(this);
    input_group->setObjectName("controlGroupCard");
    input_group->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* input_layout = new QVBoxLayout(input_group);
    input_layout->setContentsMargins(SCM(6, 6, 6, 6));
    input_layout->setSpacing(SC(4));
    input_layout->addWidget(CreateGroupCaption(QStringLiteral("输入来源"), input_group));
    input_layout->addWidget(image_path_label_);
    auto* input_buttons = new QGridLayout();
    input_buttons->setContentsMargins(0, 0, 0, 0);
    input_buttons->setHorizontalSpacing(SC(6));
    input_buttons->setVerticalSpacing(SC(6));
    input_buttons->addWidget(load_image_button_, 0, 0);
    input_buttons->addWidget(open_camera_button_, 0, 1);
    input_buttons->setColumnStretch(0, 1);
    input_buttons->setColumnStretch(1, 1);
    input_layout->addLayout(input_buttons);

    auto* detect_group = new QFrame(this);
    detect_group->setObjectName("controlGroupCard");
    detect_group->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* detect_layout = new QVBoxLayout(detect_group);
    detect_layout->setContentsMargins(SCM(6, 6, 6, 6));
    detect_layout->setSpacing(SC(5));
    detect_layout->addWidget(CreateGroupCaption(QStringLiteral("检测控制"), detect_group));

    auto* detect_buttons = new QGridLayout();
    detect_buttons->setContentsMargins(0, 0, 0, 0);
    detect_buttons->setHorizontalSpacing(SC(6));
    detect_buttons->setVerticalSpacing(SC(6));
    detect_buttons->addWidget(start_button_, 0, 0);
    detect_buttons->addWidget(plc_test_button_, 0, 1);
    detect_buttons->addWidget(plc_link_button_, 1, 0);
    detect_buttons->addWidget(stop_button_, 1, 1);
    detect_buttons->setColumnStretch(0, 1);
    detect_buttons->setColumnStretch(1, 1);
    detect_layout->addLayout(detect_buttons);

    side_layout->addWidget(input_group);
    side_layout->addSpacing(SC(6));
    side_layout->addWidget(detect_group);
}

void GraspMainWindow::buildFunctionSection(QVBoxLayout* side_layout)
{
    side_layout->addWidget(CreateSectionTitle(QStringLiteral("功能入口")));

    function_group_ = new QFrame(this);
    function_group_->setObjectName("controlGroupCard");
    function_group_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    function_layout_ = new QGridLayout(function_group_);
    function_layout_->setContentsMargins(SCM(6, 6, 6, 6));
    function_layout_->setHorizontalSpacing(SC(6));
    function_layout_->setVerticalSpacing(SC(6));
    function_layout_->setColumnStretch(0, 1);
    function_layout_->setColumnStretch(1, 1);

    side_layout->addWidget(function_group_);
}

void GraspMainWindow::buildTargetListSection(QVBoxLayout* side_layout)
{
    auto* header_layout = new QHBoxLayout();
    header_layout->setSpacing(S(8));

    target_list_back_button_ = new QPushButton(QStringLiteral("返回"), this);
    target_list_back_button_->setMinimumHeight(S(36));
    target_list_back_button_->setObjectName("secondaryActionButton");
    header_layout->addWidget(target_list_back_button_, 0, Qt::AlignLeft);
    header_layout->addWidget(CreateSectionTitle(QStringLiteral("多目标列表")), 1);

    side_layout->addLayout(header_layout);

    target_list_scroll_area_ = new QScrollArea(this);
    target_list_scroll_area_->setObjectName("targetListScrollArea");
    target_list_scroll_area_->setWidgetResizable(true);
    target_list_scroll_area_->setFrameShape(QFrame::NoFrame);
    target_list_scroll_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    target_list_content_ = new QWidget(target_list_scroll_area_);
    target_list_content_->setObjectName("targetListScrollContent");
    target_list_content_layout_ = new QVBoxLayout(target_list_content_);
    target_list_content_layout_->setContentsMargins(0, 0, 0, 0);
    target_list_content_layout_->setSpacing(S(10));
    target_list_scroll_area_->setWidget(target_list_content_);

    side_layout->addWidget(target_list_scroll_area_, 1);
}

void GraspMainWindow::applyStyles()
{
    const QString style = QString(
        "QMainWindow { background: #0b1118; }"
        "#topBar { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #2c2827, stop:1 #2a2e31);"
        " border: 1px solid #373b3f; border-radius: %1px; }"
        "#displayPanel { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #111b25, stop:0.55 #0d141d, stop:1 #162433);"
        " border: 1px solid #294055; border-radius: %2px; }"
        "#sidePanel { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #172330, stop:1 #101821);"
        " border: 1px solid #2a3f53; border-radius: %3px; }"
        "#runtimeBar { background: rgba(255,255,255,0.04); border: 1px solid #2d4458; border-radius: %4px; }"
        "#imageViewport { background: #060b10; border-radius: %5px; border: 1px solid #34506a; color: #d7e0e8;"
        " font-size: %6px; font-weight: 600; }"
        "#logoLabel { background: transparent; border: none; padding: 0px; }"
        "#brandLabel { color: #ece4dc; font-size: %7px; font-weight: 500; }"
        "#titleLabel { color: #f3eee8; font-size: %8px; font-weight: 800; }"
        "#sectionTitle { color: #eef5fb; font-size: %9px; font-weight: 700; padding-top: %10px; }"
        "#infoCard { background: rgba(255,255,255,0.045); border: 1px solid #2f465b; border-radius: %11px; }"
        "#heroInfoCard { background: rgba(74,101,125,0.34); border: 1px solid #4f7392; border-radius: %12px; }"
        "#heroSideCard { background: rgba(255,255,255,0.03); border: 1px solid #30495f; border-radius: %13px; }"
        "#groupCaption { color: #8fa9bc; font-size: %14px; font-weight: 600; }"
        "#heroPrimaryValue { color: #f5f9fd; font-size: %15px; font-weight: 800; }"
        "#heroStateValue { color: #f0f7ff; font-size: %16px; font-weight: 700; }"
        "#heroAssistValue { color: #c4d4e1; font-size: %17px; font-weight: 600; }"
        "#metricCell { background: rgba(10,18,27,0.34); border: 1px solid #456982; border-radius: %18px; }"
        "#metricName { color: #96acc0; font-size: %19px; }"
        "#metricValue { color: #f4f8fc; font-size: %20px; font-weight: 700; }"
        "#keyValueRow { background: transparent; border: none; }"
        "#keyCell { background: rgba(255,255,255,0.055); border: 1px solid #4e718a; border-radius: %56px; }"
        "#valueCell { background: rgba(8,16,25,0.38); border: 1px solid #4e718a; border-radius: %56px; }"
        "#keyLabel { color: #bfd0dc; font-size: %57px; font-weight: 600; }"
        "#valueLabel { color: #f4f8fc; font-size: %58px; font-weight: 700; }"
        "#compactMetricCell { background: rgba(255,255,255,0.035); border: 1px solid #30495f; border-radius: %21px; }"
        "#compactMetricName { color: #8ea3b4; font-size: %22px; }"
        "#compactMetricValue { color: #eef6fc; font-size: %23px; font-weight: 700; }"
        "#runtimeBadge { background: rgba(255,255,255,0.05); color: #c4d4e1; border: 1px solid #2e4a60; border-radius: %24px; padding: %25px %26px; font-size: %27px; }"
        "#runtimeStateBadge { background: rgba(34,115,178,0.18); color: #eef7ff; border: 1px solid #4b8fc5; border-radius: %28px; padding: %29px %30px; font-size: %31px; font-weight: 700; }"
        "#statusPill { background: rgba(255,255,255,0.03); border: 1px solid #2d4458; border-radius: %32px; }"
        "#statusStackItem { background: rgba(255,255,255,0.025); border: 1px solid #30495f; border-radius: %33px; }"
        "#statusStackName { color: #dbe7f1; font-size: %34px; font-weight: 600; }"
        "#controlGroupCard { background: rgba(255,255,255,0.035); border: 1px solid #30495f; border-radius: %35px; }"
        "#pathHintLabel { color: #9eb2c3; background: rgba(255,255,255,0.025); border: 1px solid #2b4256; border-radius: %36px; padding: %37px %38px; font-size: %39px; }"
        "#targetListScrollArea, #targetListScrollContent { background: transparent; border: none; }"
        "QLabel { color: #b7c7d6; }"
        "QLineEdit { background: rgba(255,255,255,0.06); color: #f3f8fc; border: 1px solid #395268; border-radius: %40px; padding: %41px %42px; }"
        "QDoubleSpinBox { background: rgba(255,255,255,0.06); color: #f3f8fc; border: 1px solid #395268; border-radius: %40px; padding: %41px %42px; }"
        "QCheckBox { color: #dbe7f1; font-size: %39px; font-weight: 600; }"
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #1d9bf0, stop:1 #1367c8); color: white; border: none; border-radius: %43px; padding: %44px %45px;"
        " font-size: %59px; font-weight: 600; }"
        "QPushButton:hover { background: #1578cb; }"
        "QPushButton:disabled { background: #516576; color: #c9d6e1; }"
        "QPushButton#primaryActionButton { background: #0090f7; color: #ffffff; font-weight: 700; }"
        "QPushButton#primaryActionButton:hover { background: #0a9dff; }"
        "QPushButton#stopActionButton { background: #5f7484; color: #eef5fb; }"
        "QPushButton#stopActionButton:hover { background: #ff9522; color: #111820; }"
        "QPushButton#secondaryActionButton { background: rgba(255,255,255,0.06); color: #ecf4fa; border: 1px solid #36546b; }"
        "QPushButton#secondaryActionButton:hover { background: rgba(255,255,255,0.12); }"
        "QToolButton { color: #d6e6f3; background: rgba(255,255,255,0.03); border: 1px solid #2d4458; border-radius: %46px; padding: %47px %48px; text-align: left; }"
        "QToolButton:checked { background: rgba(34,115,178,0.18); border-color: #4b8fc5; }"
        "QToolButton[topBarButton=\"true\"], QToolButton[windowButton=\"true\"] {"
        " background: transparent; border: 1px solid transparent; border-radius: %49px; padding: 0px; }"
        "QToolButton[topBarButton=\"true\"]:hover, QToolButton[windowButton=\"true\"]:hover {"
        " background: rgba(255,255,255,0.08); border-color: rgba(236,228,220,0.20); }"
        "QToolButton[topBarButton=\"true\"]:pressed, QToolButton[windowButton=\"true\"]:pressed {"
        " background: rgba(255,255,255,0.14); }"
        "QToolButton[closeWindowButton=\"true\"] {"
        " background: transparent; border: 1px solid transparent; border-radius: %50px; padding: 0px; }"
        "QToolButton[closeWindowButton=\"true\"]:hover {"
        " background: #d65757; border-color: #ea7d7d; }"
        "QToolButton[closeWindowButton=\"true\"]:pressed {"
        " background: #b53f3f; }"
        "#sideRailToggleButton { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #2269bf, stop:1 #164b88);"
        " color: #f1f7ff; border: 1px solid #4788d5; border-top-left-radius: %51px; border-bottom-left-radius: %52px;"
        " border-top-right-radius: %53px; border-bottom-right-radius: %54px; font-size: %55px; font-weight: 800; padding: 0px; }"
        "#sideRailToggleButton:hover { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #2b78d4, stop:1 #1a589f); }"
        "QSplitter::handle { background: transparent; }"
        "QSplitter::handle:hover { background: rgba(61, 119, 174, 0.25); }")
        .arg(S(12)).arg(S(18)).arg(S(18)).arg(S(10)).arg(S(16)).arg(S(16))
        .arg(S(15)).arg(S(18)).arg(S(14)).arg(S(1)).arg(S(11)).arg(S(12)).arg(S(10))
        .arg(S(10)).arg(S(15)).arg(S(12)).arg(S(10)).arg(S(8)).arg(S(9)).arg(S(12))
        .arg(S(8)).arg(S(8)).arg(S(11)).arg(S(8)).arg(S(2)).arg(S(6)).arg(S(9))
        .arg(S(8)).arg(S(2)).arg(S(6)).arg(S(9)).arg(S(8)).arg(S(8)).arg(S(10))
        .arg(S(10)).arg(S(8)).arg(S(3)).arg(S(5)).arg(S(9)).arg(S(7)).arg(S(4))
        .arg(S(7)).arg(S(8)).arg(S(4)).arg(S(6)).arg(S(8)).arg(S(4)).arg(S(6))
        .arg(S(17)).arg(S(17)).arg(S(12)).arg(S(12)).arg(S(6)).arg(S(6)).arg(S(16))
        .arg(SC(5)).arg(SC(10)).arg(SC(12)).arg(SC(12));
    setStyleSheet(style);
}

void GraspMainWindow::applyResponsiveLayout(bool force)
{
    const double width_scale = width() > 0 ? static_cast<double>(width()) / kDensityBaseWidth : 1.0;
    const double height_scale = height() > 0 ? static_cast<double>(height()) / kDensityBaseHeight : 1.0;
    const double next_scale = qMax(1.0, qMin(width_scale, height_scale));
    const qreal dpr = screen() ? screen()->devicePixelRatio() : 1.0;
    const int safe_margin = dpr >= 1.75 ? 6 : dpr >= 1.5 ? 8 : dpr >= 1.25 ? 10 : 12;
    const int safe_side_margin = dpr >= 1.75 ? 4 : dpr >= 1.5 ? 5 : dpr >= 1.25 ? 6 : 8;

    if (!force && std::abs(next_scale - responsive_scale_) < 0.025) {
        syncMainSplitterRatio();
        refreshSidebarCompactMetrics();
        repositionSidePanelToggle();
        return;
    }

    responsive_scale_ = next_scale;
    g_responsive_ui_scale = next_scale;
    refreshSidebarCompactMetrics();

    applyStyles();
    refreshTopBarMetrics();
    refreshActionButtonMetrics();

    if (central_panel_) {
        if (auto* root_layout = qobject_cast<QVBoxLayout*>(central_panel_->layout())) {
            root_layout->setContentsMargins(SM(safe_margin, safe_margin, safe_margin, qMax(safe_margin, 10)));
            root_layout->setSpacing(S(qMax(6, safe_margin / 2)));
        }
    }
    if (top_bar_) {
        if (auto* top_layout = qobject_cast<QGridLayout*>(top_bar_->layout())) {
            top_layout->setContentsMargins(SM(qMax(10, safe_margin), qMax(4, safe_margin / 2), qMax(10, safe_margin), qMax(4, safe_margin / 2)));
            top_layout->setHorizontalSpacing(S(qMax(6, safe_margin / 2)));
        }
    }
    if (side_panel_) {
        if (auto* side_layout = qobject_cast<QVBoxLayout*>(side_panel_->layout())) {
            side_layout->setContentsMargins(SCM(safe_side_margin, safe_side_margin, safe_side_margin, safe_side_margin));
            side_layout->setSpacing(SC(qMax(4, safe_side_margin)));
        }
    }
    if (detail_page_) {
        if (auto* detail_layout = qobject_cast<QVBoxLayout*>(detail_page_->layout())) {
            detail_layout->setContentsMargins(0, 0, 0, 0);
            detail_layout->setSpacing(SC(qMax(6, safe_side_margin)));
        }
    }
    if (target_list_page_) {
        if (auto* target_list_layout = qobject_cast<QVBoxLayout*>(target_list_page_->layout())) {
            target_list_layout->setContentsMargins(0, 0, 0, 0);
            target_list_layout->setSpacing(SC(qMax(4, safe_side_margin)));
        }
    }
    const QList<QFrame*> key_cells = findChildren<QFrame*>(QString(), Qt::FindChildrenRecursively);
    for (QFrame* frame : key_cells) {
        if (frame->property("keyMetricCell").toBool()) {
            frame->setMinimumSize(QSize(0, SC(24)));
            frame->setMaximumWidth(QWIDGETSIZE_MAX);
        } else if (frame->property("valueMetricCell").toBool()) {
            frame->setMinimumSize(QSize(0, SC(38)));
        }
    }
    const QList<QLabel*> value_labels = findChildren<QLabel*>(QString(), Qt::FindChildrenRecursively);
    for (QLabel* label : value_labels) {
        if (label->objectName() == QStringLiteral("valueLabel")) {
            label->setMinimumHeight(SC(24));
            label->setWordWrap(true);
        }
    }

    if (main_splitter_) {
        main_splitter_->setHandleWidth(S(8));
    }
    if (image_label_) {
        image_label_->setMinimumSize(SS(320, 220));
    }
    if (sidebar_toggle_button_) {
        sidebar_toggle_button_->setFixedSize(SS(kSidebarToggleWidth, kSidebarToggleHeight));
    }

    if (side_panel_) {
        side_panel_->setMinimumWidth(SidebarMinWidth());
        side_panel_->setMaximumWidth(SidebarMaxWidth());
    }
    syncMainSplitterRatio();
    refreshSidebarCompactMetrics();
    applyStyles();
    refreshActionButtonMetrics();

    syncWindowControlButtons();
    syncSidePanelToggleButton();
    repositionSidePanelToggle();
}

void GraspMainWindow::refreshTopBarMetrics()
{
    if (top_bar_) {
        top_bar_->setFixedHeight(qMax(S(46), TS(56)));
    }
    if (logo_label_) {
        logo_label_->setFixedSize(TSS(50, 42));
        const QPixmap logo_pixmap(DefaultLogoPath());
        if (!logo_pixmap.isNull()) {
            logo_label_->setPixmap(logo_pixmap.scaled(TSS(42, 34), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
    if (brand_label_) {
        brand_label_->setMinimumWidth(0);
        brand_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    }
    if (title_label_) {
        title_label_->setMinimumWidth(0);
        title_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    }

    const QSize icon_size = TSS(18, 18);
    const QSize button_size = TSS(34, 34);
    const QColor icon_color(QStringLiteral("#f0e9e1"));
    const QList<QToolButton*> buttons = {
        user_button_, settings_icon_button_, minimize_window_button_, maximize_window_button_, close_window_button_
    };
    for (QToolButton* button : buttons) {
        if (!button) {
            continue;
        }
        button->setFixedSize(button_size);
        button->setIconSize(icon_size);
    }
    if (top_right_controls_) {
        if (auto* right_layout = qobject_cast<QHBoxLayout*>(top_right_controls_->layout())) {
            right_layout->setSpacing(TS(6));
            right_layout->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        }
        constexpr int kReservedTopButtonCount = 5;
        const int controls_width = button_size.width() * kReservedTopButtonCount + TS(6) * (kReservedTopButtonCount - 1) + TS(2);
        top_right_controls_->setMinimumWidth(controls_width);
        top_right_controls_->setMaximumWidth(controls_width);
        if (auto* top_layout = qobject_cast<QGridLayout*>(top_bar_->layout())) {
            top_layout->setColumnMinimumWidth(2, controls_width);
            top_layout->setColumnStretch(0, 1);
            top_layout->setColumnStretch(1, 1);
            top_layout->setColumnStretch(2, 0);
            top_layout->setAlignment(title_label_, Qt::AlignCenter);
            top_layout->setAlignment(top_right_controls_, Qt::AlignRight | Qt::AlignVCenter);
        }
    }
    if (user_button_) {
        user_button_->setIcon(CreateAvatarIcon(icon_size, icon_color));
    }
    if (settings_icon_button_) {
        settings_icon_button_->setIcon(CreateGearIcon(icon_size, icon_color));
    }
    if (minimize_window_button_) {
        minimize_window_button_->setIcon(CreateMinimizeIcon(icon_size, icon_color));
    }
    if (close_window_button_) {
        close_window_button_->setIcon(CreateCloseIcon(icon_size, icon_color));
    }
}

void GraspMainWindow::refreshActionButtonMetrics()
{
    const QList<QPushButton*> primary_buttons = {
        load_image_button_, open_camera_button_, start_button_, plc_link_button_, plc_test_button_, stop_button_
    };
    const QList<QPushButton*> secondary_buttons = {
        display_mode_button_, target_list_button_, engineering_button_, runtime_log_button_, target_list_back_button_
    };

    for (QPushButton* button : primary_buttons) {
        if (button) {
            button->setMinimumHeight(SC(36));
        }
    }
    for (QPushButton* button : secondary_buttons) {
        if (button) {
            button->setMinimumHeight(SC(34));
        }
    }
    if (!admin_mode_ && target_list_button_) {
        target_list_button_->setMinimumHeight(SC(78));
    }
}

void GraspMainWindow::syncMainSplitterRatio()
{
    if (!main_splitter_ || !side_panel_ || syncing_splitter_sizes_) {
        return;
    }

    if (!side_panel_expanded_ || !side_panel_->isVisible()) {
        return;
    }

    const int total_width = qMax(0, main_splitter_->width() - main_splitter_->handleWidth());
    if (total_width <= 0) {
        return;
    }

    expanded_sidebar_width_ = responsiveSidebarWidth();
    const int sidebar_width = qMin(expanded_sidebar_width_, total_width);
    syncing_splitter_sizes_ = true;
    main_splitter_->setSizes({ qMax(0, total_width - sidebar_width), sidebar_width });
    syncing_splitter_sizes_ = false;
}

void GraspMainWindow::refreshSidebarCompactMetrics()
{
    const int width_basis = side_panel_ && side_panel_->width() > 0
        ? side_panel_->width()
        : responsiveSidebarWidth();
    g_sidebar_compact_scale = qBound(0.48,
                                     width_basis / static_cast<double>(S(kSidebarCompactBaseWidth)),
                                     1.0);
}

int GraspMainWindow::responsiveSidebarWidth() const
{
    const int splitter_width = main_splitter_ && main_splitter_->width() > 0
        ? main_splitter_->width() - main_splitter_->handleWidth()
        : width();
    const int proportional_width = qRound(qMax(0, splitter_width) * kSidebarWidthRatio);
    return ClampSidebarWidth(qMax(SidebarMinWidth(), proportional_width));
}

bool GraspMainWindow::isVisuallyMaximized() const
{
    return isMaximized();
}

void GraspMainWindow::logHighDpiMetrics(const QString& context) const
{
    const QScreen* target_screen = screen() ? screen() : QGuiApplication::primaryScreen();
    if (!target_screen) {
        cout << "[HighDPI] " << context.toStdString()
             << " screen=none window=" << width() << "x" << height()
             << " QT_SCALE_FACTOR=" << qgetenv("QT_SCALE_FACTOR").constData()
             << " TANKEYE_UI_SCALE=" << qgetenv("TANKEYE_UI_SCALE").constData()
             << endl;
        return;
    }

    const QRect available = target_screen->availableGeometry();
    const QSize physical = target_screen->size();
    cout << "[HighDPI] " << context.toStdString()
         << " available=" << available.width() << "x" << available.height()
         << "+" << available.x() << "+" << available.y()
         << " physical=" << physical.width() << "x" << physical.height()
         << " dpr=" << target_screen->devicePixelRatio()
         << " logicalDpi=" << target_screen->logicalDotsPerInch()
         << " window=" << width() << "x" << height()
         << " geometry=" << geometry().width() << "x" << geometry().height()
         << "+" << geometry().x() << "+" << geometry().y()
         << " QT_SCALE_FACTOR=" << qgetenv("QT_SCALE_FACTOR").constData()
         << " TANKEYE_UI_SCALE=" << qgetenv("TANKEYE_UI_SCALE").constData()
         << endl;
}

bool GraspMainWindow::hasAdminAccount() const
{
    QSettings settings(QStringLiteral("TankEye"), QStringLiteral("TankEye-Iris"));
    settings.beginGroup(QStringLiteral("admin_auth"));
    const bool result = !settings.value(QStringLiteral("username")).toString().trimmed().isEmpty() &&
                        !settings.value(QStringLiteral("password_hash")).toString().trimmed().isEmpty() &&
                        !settings.value(QStringLiteral("salt")).toString().trimmed().isEmpty();
    settings.endGroup();
    return result;
}

QString GraspMainWindow::adminUsername() const
{
    QSettings settings(QStringLiteral("TankEye"), QStringLiteral("TankEye-Iris"));
    settings.beginGroup(QStringLiteral("admin_auth"));
    const QString username = settings.value(QStringLiteral("username")).toString().trimmed();
    settings.endGroup();
    return username;
}

bool GraspMainWindow::setAdminCredentials(const QString& username, const QString& password, QString* error_message)
{
    const QString trimmed_username = username.trimmed();
    if (trimmed_username.isEmpty()) {
        if (error_message) {
            *error_message = QStringLiteral("管理员账号不能为空。");
        }
        return false;
    }
    if (password.size() < 4) {
        if (error_message) {
            *error_message = QStringLiteral("管理员密码至少需要 4 位。");
        }
        return false;
    }

    const QByteArray salt = RandomSalt();
    QSettings settings(QStringLiteral("TankEye"), QStringLiteral("TankEye-Iris"));
    settings.beginGroup(QStringLiteral("admin_auth"));
    settings.setValue(QStringLiteral("username"), trimmed_username);
    settings.setValue(QStringLiteral("salt"), QString::fromLatin1(salt.toBase64()));
    settings.setValue(QStringLiteral("password_hash"), HashAdminPassword(password, salt));
    settings.endGroup();
    settings.sync();
    return true;
}

bool GraspMainWindow::changeAdminCredentials(const QString& current_password,
                                             const QString& username,
                                             const QString& new_password,
                                             QString* error_message)
{
    if (hasAdminAccount() && !validateAdminCredentials(adminUsername(), current_password)) {
        if (error_message) {
            *error_message = QStringLiteral("当前密码不正确。");
        }
        return false;
    }
    return setAdminCredentials(username, new_password, error_message);
}

bool GraspMainWindow::validateAdminCredentials(const QString& username, const QString& password) const
{
    QSettings settings(QStringLiteral("TankEye"), QStringLiteral("TankEye-Iris"));
    settings.beginGroup(QStringLiteral("admin_auth"));
    const QString saved_username = settings.value(QStringLiteral("username")).toString().trimmed();
    const QByteArray salt = QByteArray::fromBase64(settings.value(QStringLiteral("salt")).toString().toLatin1());
    const QString saved_hash = settings.value(QStringLiteral("password_hash")).toString();
    settings.endGroup();
    if (saved_username.isEmpty() || saved_hash.isEmpty() || salt.isEmpty()) {
        return false;
    }
    return username.trimmed() == saved_username && HashAdminPassword(password, salt) == saved_hash;
}

QString GraspMainWindow::rememberedAdminPassword() const
{
    QSettings settings(QStringLiteral("TankEye"), QStringLiteral("TankEye-Iris"));
    settings.beginGroup(QStringLiteral("admin_auth"));
    const bool remember = settings.value(QStringLiteral("remember_password"), false).toBool();
    const QString encoded = settings.value(QStringLiteral("remembered_password")).toString();
    settings.endGroup();
    return remember ? DecodeRememberedPassword(encoded) : QString();
}

void GraspMainWindow::saveRememberedAdminPassword(bool remember, const QString& password)
{
    QSettings settings(QStringLiteral("TankEye"), QStringLiteral("TankEye-Iris"));
    settings.beginGroup(QStringLiteral("admin_auth"));
    settings.setValue(QStringLiteral("remember_password"), remember);
    if (remember) {
        settings.setValue(QStringLiteral("remembered_password"), EncodeRememberedPassword(password));
    } else {
        settings.remove(QStringLiteral("remembered_password"));
    }
    settings.endGroup();
    settings.sync();
}

void GraspMainWindow::setAdminMode(bool enabled)
{
    if (admin_mode_ == enabled) {
        refreshAdminModeUi();
        return;
    }

    admin_mode_ = enabled;
    if (!admin_mode_ && engineering_settings_dialog_) {
        engineering_settings_dialog_->close();
    }
    refreshAdminModeUi();
    updateStatusMessage(admin_mode_ ? QStringLiteral("已进入管理员模式。")
                                    : QStringLiteral("已退出管理员模式。"),
                        3000);
}

void GraspMainWindow::refreshAdminModeUi()
{
    if (!function_layout_ || !target_list_button_) {
        return;
    }

    while (QLayoutItem* item = function_layout_->takeAt(0)) {
        delete item;
    }

    if (settings_icon_button_) {
        settings_icon_button_->setVisible(admin_mode_);
    }
    if (user_button_) {
        user_button_->setToolTip(admin_mode_ ? QStringLiteral("退出管理员模式")
                                             : QStringLiteral("管理员登录"));
    }

    if (admin_mode_) {
        display_mode_button_->setVisible(true);
        engineering_button_->setVisible(true);
        runtime_log_button_->setVisible(true);
        target_list_button_->setVisible(true);
        target_list_button_->setMinimumHeight(SC(34));

        function_layout_->addWidget(display_mode_button_, 0, 0);
        function_layout_->addWidget(target_list_button_, 0, 1);
        function_layout_->addWidget(engineering_button_, 1, 0);
        function_layout_->addWidget(runtime_log_button_, 1, 1);
    } else {
        display_mode_button_->setVisible(false);
        engineering_button_->setVisible(false);
        runtime_log_button_->setVisible(false);
        target_list_button_->setVisible(true);
        target_list_button_->setMinimumHeight(SC(78));
        function_layout_->addWidget(target_list_button_, 0, 0, 2, 2);
    }

    refreshActionButtonMetrics();
    if (!admin_mode_) {
        target_list_button_->setMinimumHeight(SC(78));
    }
    refreshTopBarMetrics();
}

void GraspMainWindow::showCreateAdminAccountDialog()
{
    const QString machine_code = AdminAuthMachineCode();
    QString auth_secret_warning;
    const QString auth_secret = AdminAuthSecret(&auth_secret_warning);

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("创建管理员账号"));
    dialog.setWindowFlags(Qt::Window | Qt::Dialog);
    dialog.setStyleSheet(AdminAuthDialogStyle());

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(SM(18, 18, 18, 18));
    layout->setSpacing(S(12));

    auto* hint_label = new QLabel(QStringLiteral("首次授权：把机器码发给工程师，拿到授权码后才能创建管理员。"), &dialog);
    hint_label->setObjectName("authHintLabel");
    hint_label->setWordWrap(true);
    layout->addWidget(hint_label);

    auto* auth_grid = new QGridLayout();
    auth_grid->setHorizontalSpacing(S(8));
    auth_grid->setVerticalSpacing(S(10));
    auth_grid->setColumnStretch(0, 0);
    auth_grid->setColumnStretch(1, 1);
    auth_grid->setColumnStretch(2, 0);

    auto* machine_code_edit = new QLineEdit(machine_code, &dialog);
    machine_code_edit->setReadOnly(true);
    machine_code_edit->setCursorPosition(0);
    auto* copy_machine_button = new QPushButton(QStringLiteral("复制"), &dialog);
    copy_machine_button->setObjectName("secondaryAuthButton");
    auth_grid->addWidget(CreateAuthFormLabel(QStringLiteral("机 器 码："), &dialog), 0, 0);
    auth_grid->addWidget(machine_code_edit, 0, 1);
    auth_grid->addWidget(copy_machine_button, 0, 2);

    auto* auth_code_edit = new QLineEdit(&dialog);
    auth_code_edit->setPlaceholderText(QStringLiteral("请输入工程师提供的授权码"));
    auth_grid->addWidget(CreateAuthFormLabel(QStringLiteral("授 权 码："), &dialog), 1, 0);
    auth_grid->addWidget(auth_code_edit, 1, 1);

    auto* username_edit = new QLineEdit(&dialog);
    auto* password_edit = new QLineEdit(&dialog);
    auto* confirm_edit = new QLineEdit(&dialog);
    password_edit->setPlaceholderText(QStringLiteral("设置管理员密码"));
    confirm_edit->setPlaceholderText(QStringLiteral("再次输入密码"));
    auth_grid->addWidget(CreateAuthFormLabel(QStringLiteral("账      号："), &dialog), 2, 0);
    auth_grid->addWidget(username_edit, 2, 1);
    auth_grid->addWidget(CreateAuthFormLabel(QStringLiteral("密      码："), &dialog), 3, 0);
    auth_grid->addWidget(password_edit, 3, 1);
    auth_grid->addWidget(CreatePasswordVisibilityButton(password_edit, &dialog), 3, 2);
    auth_grid->addWidget(CreateAuthFormLabel(QStringLiteral("确认密码："), &dialog), 4, 0);
    auth_grid->addWidget(confirm_edit, 4, 1);
    auth_grid->addWidget(CreatePasswordVisibilityButton(confirm_edit, &dialog), 4, 2);
    layout->addLayout(auth_grid);

    auto* copy_toast = new QLabel(QStringLiteral("机器码已复制"), &dialog);
    copy_toast->setObjectName("authCopyToast");
    copy_toast->setVisible(false);
    layout->addWidget(copy_toast, 0, Qt::AlignRight);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("创建并登录"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    layout->addWidget(buttons);

    connect(copy_machine_button, &QPushButton::clicked, &dialog, [machine_code_edit, copy_toast]() {
        QApplication::clipboard()->setText(machine_code_edit->text());
        ShowCopyToast(copy_toast);
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        if (auth_code_edit->text().trimmed().isEmpty()) {
            QMessageBox::warning(&dialog, QStringLiteral("创建失败"), QStringLiteral("请输入授权码。"));
            return;
        }
        if (password_edit->text() != confirm_edit->text()) {
            QMessageBox::warning(&dialog, QStringLiteral("创建失败"), QStringLiteral("两次输入的密码不一致。"));
            return;
        }
        if (!VerifyAdminAuthCode(machine_code_edit->text(), auth_code_edit->text(), auth_secret, QStringLiteral("INIT"))) {
            QString message = QStringLiteral("授权码无效。");
            if (!auth_secret_warning.isEmpty()) {
                message += QStringLiteral("\n\n当前为开发默认密钥，测试授权码应为：%1")
                               .arg(BuildAdminAuthCode(machine_code_edit->text(), auth_secret, QStringLiteral("INIT")));
            }
            QMessageBox::warning(&dialog, QStringLiteral("创建失败"), message);
            return;
        }
        QString error;
        if (!setAdminCredentials(username_edit->text(), password_edit->text(), &error)) {
            QMessageBox::warning(&dialog, QStringLiteral("创建失败"), error);
            return;
        }
        saveRememberedAdminPassword(false, QString());
        dialog.accept();
    });

    if (dialog.exec() == QDialog::Accepted) {
        setAdminMode(true);
    }
}

void GraspMainWindow::showAdminLoginDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("管理员登录"));
    dialog.setWindowFlags(Qt::Window | Qt::Dialog);
    dialog.setStyleSheet(AdminAuthDialogStyle());

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(SM(18, 18, 18, 18));
    layout->setSpacing(S(12));

    auto* hint_label = new QLabel(QStringLiteral("管理员模式用于调试入口；普通模式下只保留目标列表。"), &dialog);
    hint_label->setObjectName("authHintLabel");
    hint_label->setWordWrap(true);
    layout->addWidget(hint_label);

    auto* form = new QGridLayout();
    form->setHorizontalSpacing(S(8));
    form->setVerticalSpacing(S(10));
    form->setColumnStretch(0, 0);
    form->setColumnStretch(1, 1);
    form->setColumnStretch(2, 0);
    auto* username_edit = new QLineEdit(adminUsername(), &dialog);
    auto* password_edit = new QLineEdit(rememberedAdminPassword(), &dialog);
    auto* remember_check = new QCheckBox(QStringLiteral("记住密码"), &dialog);
    remember_check->setChecked(!password_edit->text().isEmpty());
    form->addWidget(CreateAuthFormLabel(QStringLiteral("账      号："), &dialog), 0, 0);
    form->addWidget(username_edit, 0, 1);
    form->addWidget(CreateAuthFormLabel(QStringLiteral("密      码："), &dialog), 1, 0);
    form->addWidget(password_edit, 1, 1);
    form->addWidget(CreatePasswordVisibilityButton(password_edit, &dialog), 1, 2);
    form->addWidget(remember_check, 2, 1);
    layout->addLayout(form);

    auto* button_row = new QHBoxLayout();
    button_row->setSpacing(S(8));
    auto* reset_button = new QPushButton(QStringLiteral("忘记密码"), &dialog);
    reset_button->setObjectName("secondaryAuthButton");
    auto* login_button = new QPushButton(QStringLiteral("登录"), &dialog);
    auto* cancel_button = new QPushButton(QStringLiteral("取消"), &dialog);
    cancel_button->setObjectName("secondaryAuthButton");
    button_row->addWidget(reset_button, 0, Qt::AlignLeft);
    button_row->addStretch(1);
    button_row->addWidget(login_button);
    button_row->addWidget(cancel_button);
    layout->addLayout(button_row);

    connect(reset_button, &QPushButton::clicked, &dialog, [this, &dialog]() {
        if (showAdminResetDialog()) {
            dialog.accept();
        }
    });
    connect(cancel_button, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(login_button, &QPushButton::clicked, &dialog, [&]() {
        if (!validateAdminCredentials(username_edit->text(), password_edit->text())) {
            QMessageBox::warning(&dialog, QStringLiteral("登录失败"), QStringLiteral("管理员账号或密码错误。"));
            return;
        }
        saveRememberedAdminPassword(remember_check->isChecked(), password_edit->text());
        dialog.accept();
    });

    if (dialog.exec() == QDialog::Accepted) {
        setAdminMode(true);
    }
}

bool GraspMainWindow::showAdminResetDialog()
{
    const QString machine_code = AdminAuthMachineCode();
    QString auth_secret_warning;
    const QString auth_secret = AdminAuthSecret(&auth_secret_warning);

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("重置管理员"));
    dialog.setWindowFlags(Qt::Window | Qt::Dialog);
    dialog.setStyleSheet(AdminAuthDialogStyle());

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(SM(18, 18, 18, 18));
    layout->setSpacing(S(12));

    auto* hint_label = new QLabel(QStringLiteral("忘记密码时，使用机器码和重置码重新设置管理员账号。"), &dialog);
    hint_label->setObjectName("authHintLabel");
    hint_label->setWordWrap(true);
    layout->addWidget(hint_label);

    auto* reset_grid = new QGridLayout();
    reset_grid->setHorizontalSpacing(S(8));
    reset_grid->setVerticalSpacing(S(10));
    reset_grid->setColumnStretch(0, 0);
    reset_grid->setColumnStretch(1, 1);
    reset_grid->setColumnStretch(2, 0);

    auto* machine_code_edit = new QLineEdit(machine_code, &dialog);
    machine_code_edit->setReadOnly(true);
    machine_code_edit->setCursorPosition(0);
    auto* copy_machine_button = new QPushButton(QStringLiteral("复制"), &dialog);
    copy_machine_button->setObjectName("secondaryAuthButton");
    reset_grid->addWidget(CreateAuthFormLabel(QStringLiteral("机 器 码："), &dialog), 0, 0);
    reset_grid->addWidget(machine_code_edit, 0, 1);
    reset_grid->addWidget(copy_machine_button, 0, 2);

    auto* reset_code_edit = new QLineEdit(&dialog);
    reset_code_edit->setPlaceholderText(QStringLiteral("请输入维护重置码"));
    reset_grid->addWidget(CreateAuthFormLabel(QStringLiteral("重置码："), &dialog), 1, 0);
    reset_grid->addWidget(reset_code_edit, 1, 1);

    auto* username_edit = new QLineEdit(adminUsername(), &dialog);
    auto* password_edit = new QLineEdit(&dialog);
    auto* confirm_edit = new QLineEdit(&dialog);
    password_edit->setPlaceholderText(QStringLiteral("新管理员密码"));
    confirm_edit->setPlaceholderText(QStringLiteral("再次输入新密码"));
    reset_grid->addWidget(CreateAuthFormLabel(QStringLiteral("账      号："), &dialog), 2, 0);
    reset_grid->addWidget(username_edit, 2, 1);
    reset_grid->addWidget(CreateAuthFormLabel(QStringLiteral("密      码："), &dialog), 3, 0);
    reset_grid->addWidget(password_edit, 3, 1);
    reset_grid->addWidget(CreatePasswordVisibilityButton(password_edit, &dialog), 3, 2);
    reset_grid->addWidget(CreateAuthFormLabel(QStringLiteral("确认密码："), &dialog), 4, 0);
    reset_grid->addWidget(confirm_edit, 4, 1);
    reset_grid->addWidget(CreatePasswordVisibilityButton(confirm_edit, &dialog), 4, 2);
    layout->addLayout(reset_grid);

    auto* copy_toast = new QLabel(QStringLiteral("机器码已复制"), &dialog);
    copy_toast->setObjectName("authCopyToast");
    copy_toast->setVisible(false);
    layout->addWidget(copy_toast, 0, Qt::AlignRight);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("重置并登录"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    layout->addWidget(buttons);

    connect(copy_machine_button, &QPushButton::clicked, &dialog, [machine_code_edit, copy_toast]() {
        QApplication::clipboard()->setText(machine_code_edit->text());
        ShowCopyToast(copy_toast);
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        if (reset_code_edit->text().trimmed().isEmpty()) {
            QMessageBox::warning(&dialog, QStringLiteral("重置失败"), QStringLiteral("请输入重置码。"));
            return;
        }
        if (password_edit->text() != confirm_edit->text()) {
            QMessageBox::warning(&dialog, QStringLiteral("重置失败"), QStringLiteral("两次输入的新密码不一致。"));
            return;
        }
        if (!VerifyAdminAuthCode(machine_code_edit->text(), reset_code_edit->text(), auth_secret, QStringLiteral("RESET"))) {
            QString message = QStringLiteral("重置码无效。");
            if (!auth_secret_warning.isEmpty()) {
                message += QStringLiteral("\n\n当前为开发默认密钥，测试重置码应为：%1")
                               .arg(BuildAdminAuthCode(machine_code_edit->text(), auth_secret, QStringLiteral("RESET")));
            }
            QMessageBox::warning(&dialog, QStringLiteral("重置失败"), message);
            return;
        }
        QString error;
        if (!setAdminCredentials(username_edit->text(), password_edit->text(), &error)) {
            QMessageBox::warning(&dialog, QStringLiteral("重置失败"), error);
            return;
        }
        saveRememberedAdminPassword(false, QString());
        dialog.accept();
    });

    if (dialog.exec() == QDialog::Accepted) {
        setAdminMode(true);
        return true;
    }
    return false;
}

void GraspMainWindow::handleUserButtonClicked()
{
    if (admin_mode_) {
        const auto result = QMessageBox::question(this,
                                                  QStringLiteral("管理员模式"),
                                                  QStringLiteral("是否退出管理员模式？"),
                                                  QMessageBox::Yes | QMessageBox::No,
                                                  QMessageBox::No);
        if (result == QMessageBox::Yes) {
            setAdminMode(false);
        }
        return;
    }

    if (hasAdminAccount()) {
        showAdminLoginDialog();
    } else {
        showCreateAdminAccountDialog();
    }
}

void GraspMainWindow::bindActions()
{
    connect(load_image_button_, &QPushButton::clicked, this, [this]() { loadImage(); });
    connect(open_camera_button_, &QPushButton::clicked, this, [this]() { openCamera(); });
    connect(start_button_, &QPushButton::clicked, this, [this]() { startDetection(); });
    connect(plc_link_button_, &QPushButton::clicked, this, [this]() { togglePlcLinkMode(); });
    connect(plc_test_button_, &QPushButton::clicked, this, [this]() { writePlcTestValues(); });
    connect(stop_button_, &QPushButton::clicked, this, [this]() { closeCamera(); });
    connect(display_mode_button_, &QPushButton::clicked, this, [this]() { setShowAllDetections(!show_all_detections_); });
    connect(target_list_button_, &QPushButton::clicked, this, [this]() { showTargetListPage(); });
    connect(target_list_back_button_, &QPushButton::clicked, this, [this]() { showDetailPage(); });
    connect(engineering_button_, &QPushButton::clicked, this, [this]() { openEngineeringSettings(); });
    connect(runtime_log_button_, &QPushButton::clicked, this, [this]() { showRuntimeLogs(); });
    connect(user_button_, &QToolButton::clicked, this, [this]() { handleUserButtonClicked(); });
    connect(settings_icon_button_, &QToolButton::clicked, this, [this]() { openEngineeringSettings(); });
    connect(minimize_window_button_, &QToolButton::clicked, this, [this]() { showMinimized(); });
    connect(maximize_window_button_, &QToolButton::clicked, this, [this]() {
        if (isMaximized()) {
            showNormal();
        } else {
            showMaximized();
        }
        syncWindowControlButtons();
        repositionSidePanelToggle();
    });
    connect(close_window_button_, &QToolButton::clicked, this, [this]() { close(); });
    connect(sidebar_toggle_button_, &QToolButton::clicked, this, [this]() { toggleSidePanel(); });
    connect(plc_poll_timer_, &QTimer::timeout, this, [this]() { pollPlcTrigger(); });
    connect(main_splitter_, &QSplitter::splitterMoved, this, [this](int, int) {
        if (syncing_splitter_sizes_) {
            return;
        }
        if (!side_panel_expanded_ || !side_panel_) {
            repositionSidePanelToggle();
            return;
        }

        syncMainSplitterRatio();
        refreshSidebarCompactMetrics();
        applyStyles();
        refreshActionButtonMetrics();
        repositionSidePanelToggle();
    });
}

void GraspMainWindow::syncWindowControlButtons()
{
    if (!maximize_window_button_) {
        return;
    }

    const QSize icon_size = maximize_window_button_->iconSize().isValid() ? maximize_window_button_->iconSize() : QSize(18, 18);
    const QColor icon_color(QStringLiteral("#f0e9e1"));
    maximize_window_button_->setIcon(isMaximized() ? CreateRestoreIcon(icon_size, icon_color)
                                                   : CreateMaximizeIcon(icon_size, icon_color));
    maximize_window_button_->setToolTip(isVisuallyMaximized() ? QStringLiteral("还原窗口")
                                                              : QStringLiteral("最大化"));
}

void GraspMainWindow::syncSidePanelToggleButton()
{
    if (!sidebar_toggle_button_) {
        return;
    }

    sidebar_toggle_button_->setText(side_panel_expanded_ ? ">" : "<");
    sidebar_toggle_button_->setToolTip(side_panel_expanded_ ? QStringLiteral("收起右侧栏")
                                                            : QStringLiteral("展开右侧栏"));
}

void GraspMainWindow::toggleSidePanel()
{
    applySidePanelState(!side_panel_expanded_);
}

void GraspMainWindow::applySidePanelState(bool expanded)
{
    if (!main_splitter_ || !side_panel_) {
        return;
    }

    if (!expanded && side_panel_expanded_ && side_panel_->isVisible()) {
        expanded_sidebar_width_ = ClampSidebarWidth(side_panel_->width());
    }

    side_panel_expanded_ = expanded;
    syncSidePanelToggleButton();

    if (expanded) {
        expanded_sidebar_width_ = responsiveSidebarWidth();
        side_panel_->setMinimumWidth(SidebarMinWidth());
        side_panel_->setMaximumWidth(SidebarMaxWidth());
        side_panel_->show();
        if (main_splitter_->handle(1)) {
            main_splitter_->handle(1)->show();
        }

        syncMainSplitterRatio();
    } else {
        main_splitter_->setSizes({ qMax(0, main_splitter_->width()), 0 });
        side_panel_->hide();
        if (main_splitter_->handle(1)) {
            main_splitter_->handle(1)->hide();
        }
    }

    refreshSidebarCompactMetrics();
    applyStyles();
    refreshActionButtonMetrics();
    repositionSidePanelToggle();
}

void GraspMainWindow::repositionSidePanelToggle()
{
    if (!sidebar_toggle_button_ || !central_panel_) {
        return;
    }

    const int x = qMax(0, central_panel_->width() - sidebar_toggle_button_->width() - S(2));
    const int y = qMax(S(70), (central_panel_->height() - sidebar_toggle_button_->height()) / 2);
    sidebar_toggle_button_->move(x, y);
    sidebar_toggle_button_->raise();
}

void GraspMainWindow::waitForBackgroundJobs()
{
    if (model_load_watcher_ && !model_load_watcher_->isFinished()) {
        model_load_watcher_->waitForFinished();
    }
    if (image_detection_watcher_ && !image_detection_watcher_->isFinished()) {
        image_detection_watcher_->waitForFinished();
    }
    if (plc_poll_watcher_ && !plc_poll_watcher_->isFinished()) {
        plc_poll_watcher_->waitForFinished();
    }
    if (plc_detection_watcher_ && !plc_detection_watcher_->isFinished()) {
        plc_detection_watcher_->waitForFinished();
    }
    if (plc_test_watcher_ && !plc_test_watcher_->isFinished()) {
        plc_test_watcher_->waitForFinished();
    }
    if (image_load_watcher_ && !image_load_watcher_->isFinished()) {
        image_load_watcher_->waitForFinished();
    }
}

void GraspMainWindow::resetCameraUiCounters()
{
    camera_ui_update_pending_ = false;
    camera_ui_seen_count_ = 0;
    camera_ui_skipped_count_ = 0;
    camera_ui_rendered_count_ = 0;
}

void GraspMainWindow::stopPlcLinkState(RuntimeWorkflowState next_state)
{
    plc_runtime_state_.stop(next_state);
    if (plc_poll_timer_) {
        plc_poll_timer_->stop();
    }
}

void GraspMainWindow::startPlcPollingState()
{
    plc_runtime_state_.startPolling();
    if (plc_poll_timer_) {
        plc_poll_timer_->start();
    }
}

void GraspMainWindow::openEngineeringSettings()
{
    if (!admin_mode_) {
        if (hasAdminAccount()) {
            showAdminLoginDialog();
        } else {
            showCreateAdminAccountDialog();
        }
        if (!admin_mode_) {
            return;
        }
    }
    if (engineering_settings_dialog_controller_) {
        engineering_settings_dialog_controller_->show();
    }
}

void GraspMainWindow::showRuntimeLogs()
{
    {
        auto* dialog = new QDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle(QString::fromUtf8("运行日志"));
        dialog->resize(SS(1080, 680));

        auto* layout = new QVBoxLayout(dialog);
        layout->setContentsMargins(SM(14, 14, 14, 14));
        layout->setSpacing(S(8));

        auto* selector_row = new QHBoxLayout();
        selector_row->setSpacing(S(8));
        selector_row->addWidget(new QLabel(QString::fromUtf8("日志文件"), dialog));
        auto* log_selector = new QComboBox(dialog);
        log_selector->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        selector_row->addWidget(log_selector, 1);
        layout->addLayout(selector_row);

        auto* filter_row = new QHBoxLayout();
        filter_row->setSpacing(S(8));
        auto* search_edit = new QLineEdit(dialog);
        search_edit->setPlaceholderText(QString::fromUtf8("搜索关键词，例如 error / warning / PLC / camera"));
        auto* level_selector = new QComboBox(dialog);
        level_selector->addItem(QString::fromUtf8("全部"), QString());
        level_selector->addItem(QStringLiteral("ERROR"), QStringLiteral("error|failed|fail|失败"));
        level_selector->addItem(QStringLiteral("WARNING"), QStringLiteral("warning|warn|警告"));
        level_selector->addItem(QStringLiteral("PLC"), QStringLiteral("plc|modbus|register|d500|d502|d506"));
        level_selector->addItem(QString::fromUtf8("相机"), QStringLiteral("camera|hik|frame|相机"));
        level_selector->addItem(QString::fromUtf8("模型"), QStringLiteral("model|openvino|obb|seg|模型"));
        auto* from_time = new QDateTimeEdit(dialog);
        auto* to_time = new QDateTimeEdit(dialog);
        const QDateTime now = QDateTime::currentDateTime();
        from_time->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        to_time->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        from_time->setCalendarPopup(true);
        to_time->setCalendarPopup(true);
        from_time->setDateTime(now.addDays(-7));
        to_time->setDateTime(now.addDays(1));
        filter_row->addWidget(search_edit, 2);
        filter_row->addWidget(level_selector);
        filter_row->addWidget(new QLabel(QString::fromUtf8("从"), dialog));
        filter_row->addWidget(from_time);
        filter_row->addWidget(new QLabel(QString::fromUtf8("到"), dialog));
        filter_row->addWidget(to_time);
        layout->addLayout(filter_row);

        auto* log_location_label = new QLabel(dialog);
        log_location_label->setWordWrap(true);
        log_location_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(log_location_label);

        auto* log_view = new QTextEdit(dialog);
        log_view->setReadOnly(true);
        log_view->setLineWrapMode(QTextEdit::NoWrap);
        log_view->setObjectName("logTextView");
        layout->addWidget(log_view, 1);

        auto* page_row = new QHBoxLayout();
        page_row->setSpacing(S(8));
        auto* prev_button = new QPushButton(QString::fromUtf8("上一页"), dialog);
        auto* next_button = new QPushButton(QString::fromUtf8("下一页"), dialog);
        auto* latest_button = new QPushButton(QString::fromUtf8("最新"), dialog);
        auto* page_label = new QLabel(dialog);
        auto* close_button = new QPushButton(QString::fromUtf8("关闭"), dialog);
        page_row->addWidget(prev_button);
        page_row->addWidget(next_button);
        page_row->addWidget(latest_button);
        page_row->addWidget(page_label, 1);
        page_row->addWidget(close_button);
        layout->addLayout(page_row);

        dialog->setStyleSheet(QString(
            "QDialog { background: #0f141a; color: #e6edf7; }"
            "QLabel { color: #dbe7f1; font-size: %1px; font-weight: 600; }"
            "QComboBox, QDateTimeEdit, QLineEdit { background: #181f29; color: #f4f8fc; border: 1px solid #354b60; border-radius: %2px; padding: %3px %4px; font-size: %1px; }"
            "QTextEdit#logTextView { background: #080d12; color: #dce7f1; border: 1px solid #27303d; border-radius: %5px; font-family: Consolas, 'Courier New'; font-size: %6px; }"
            "QPushButton { background: #2a4365; color: #ecf4fa; border: 1px solid #36546b; border-radius: %7px; padding: %8px %9px; font-size: %1px; font-weight: 600; }")
            .arg(S(13)).arg(S(6)).arg(S(6)).arg(S(8)).arg(S(8)).arg(S(12)).arg(S(7)).arg(S(6)).arg(S(12)));

        const QDir app_log_dir(QCoreApplication::applicationDirPath() + QStringLiteral("/logs"));
        const QDir cwd_log_dir(QDir::current().filePath(QStringLiteral("logs")));
        const QString env_log_file = QString::fromLocal8Bit(qgetenv("TANKEYE_LOG_FILE")).trimmed();
        const auto add_log_file = [log_selector](const QString& path) {
            const QFileInfo info(path);
            if (!info.exists() || !info.isFile()) {
                return;
            }
            for (int i = 0; i < log_selector->count(); ++i) {
                if (log_selector->itemData(i).toString() == info.absoluteFilePath()) {
                    return;
                }
            }
            const QString label = QStringLiteral("%1  %2")
                                      .arg(info.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")))
                                      .arg(info.absoluteFilePath());
            log_selector->addItem(label, info.absoluteFilePath());
        };
        const auto add_log_dir = [add_log_file](const QDir& dir) {
            const QFileInfoList logs = dir.entryInfoList({ QStringLiteral("*.log") }, QDir::Files, QDir::Time);
            for (const QFileInfo& info : logs) {
                add_log_file(info.absoluteFilePath());
            }
        };
        if (!env_log_file.isEmpty()) {
            add_log_file(env_log_file);
        }
        add_log_dir(app_log_dir);
        add_log_dir(cwd_log_dir);
        log_location_label->setText(QString::fromUtf8("日志位置：%1；%2")
                                        .arg(app_log_dir.absolutePath(), cwd_log_dir.absolutePath()));

        const int page_size = 1000;
        const qint64 max_read_bytes = 4 * 1024 * 1024;
        auto* page_index = new int(0);
        auto* latest_mode = new bool(true);
        const auto parse_line_time = [](const QString& line) -> QDateTime {
            int index = line.indexOf(QStringLiteral("20"));
            while (index >= 0 && index + 19 <= line.size()) {
                const QString candidate = line.mid(index, 19);
                QDateTime parsed = QDateTime::fromString(candidate, Qt::ISODate);
                if (!parsed.isValid()) {
                    parsed = QDateTime::fromString(candidate, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
                }
                if (parsed.isValid()) {
                    return parsed;
                }
                index = line.indexOf(QStringLiteral("20"), index + 1);
            }
            return QDateTime();
        };
        const auto refresh_log = [=]() {
            const QString path = log_selector->currentData().toString();
            if (path.isEmpty()) {
                log_view->setPlainText(QString::fromUtf8("暂无日志。\n%1").arg(log_location_label->text()));
                return;
            }
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                log_view->setPlainText(QString::fromUtf8("日志打开失败：%1\n原因：%2").arg(path, file.errorString()));
                return;
            }
            if (file.size() > max_read_bytes) {
                file.seek(file.size() - max_read_bytes);
                file.readLine();
            }
            const QStringList raw_lines = QString::fromLocal8Bit(file.readAll()).split(QLatin1Char('\n'));
            const QString keyword = search_edit->text().trimmed();
            const QStringList level_terms = level_selector->currentData().toString().split(QLatin1Char('|'), Qt::SkipEmptyParts);
            QStringList filtered;
            filtered.reserve(raw_lines.size());
            for (const QString& raw_line : raw_lines) {
                const QString line = raw_line.trimmed();
                if (line.isEmpty()) {
                    continue;
                }
                if (!keyword.isEmpty() && !line.contains(keyword, Qt::CaseInsensitive)) {
                    continue;
                }
                bool level_match = level_terms.isEmpty();
                for (const QString& term : level_terms) {
                    if (line.contains(term, Qt::CaseInsensitive)) {
                        level_match = true;
                        break;
                    }
                }
                if (!level_match) {
                    continue;
                }
                const QDateTime line_time = parse_line_time(line);
                if (line_time.isValid() && (line_time < from_time->dateTime() || line_time > to_time->dateTime())) {
                    continue;
                }
                filtered.push_back(line);
            }
            const int page_count = qMax(1, (filtered.size() + page_size - 1) / page_size);
            if (*latest_mode) {
                *page_index = page_count - 1;
            }
            *page_index = qBound(0, *page_index, page_count - 1);
            log_view->setPlainText(filtered.mid((*page_index) * page_size, page_size).join(QStringLiteral("\n")));
            log_view->moveCursor(QTextCursor::End);
            page_label->setText(QString::fromUtf8("第 %1/%2 页，每页 %3 行；匹配 %4 行；当前：%5")
                                    .arg(*page_index + 1)
                                    .arg(page_count)
                                    .arg(page_size)
                                    .arg(filtered.size())
                                    .arg(QFileInfo(path).fileName()));
        };

        connect(log_selector, QOverload<int>::of(&QComboBox::currentIndexChanged), dialog, [=](int) { *latest_mode = true; refresh_log(); });
        connect(search_edit, &QLineEdit::textChanged, dialog, [=]() { *latest_mode = true; refresh_log(); });
        connect(level_selector, QOverload<int>::of(&QComboBox::currentIndexChanged), dialog, [=](int) { *latest_mode = true; refresh_log(); });
        connect(from_time, &QDateTimeEdit::dateTimeChanged, dialog, [=](const QDateTime&) { *latest_mode = true; refresh_log(); });
        connect(to_time, &QDateTimeEdit::dateTimeChanged, dialog, [=](const QDateTime&) { *latest_mode = true; refresh_log(); });
        connect(prev_button, &QPushButton::clicked, dialog, [=]() { *latest_mode = false; --(*page_index); refresh_log(); });
        connect(next_button, &QPushButton::clicked, dialog, [=]() { *latest_mode = false; ++(*page_index); refresh_log(); });
        connect(latest_button, &QPushButton::clicked, dialog, [=]() { *latest_mode = true; refresh_log(); });
        connect(close_button, &QPushButton::clicked, dialog, &QDialog::accept);

        refresh_log();
        auto* auto_refresh_timer = new QTimer(dialog);
        auto_refresh_timer->setInterval(500);
        connect(auto_refresh_timer, &QTimer::timeout, dialog, refresh_log);
        auto_refresh_timer->start();
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
        return;
    }

    auto* dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QStringLiteral("运行日志"));
    dialog->resize(SS(880, 560));

    auto* layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(SM(14, 14, 14, 14));
    layout->setSpacing(S(10));

    auto* selector_row = new QHBoxLayout();
    selector_row->setSpacing(S(8));
    auto* selector_label = new QLabel(QStringLiteral("日志文件"), dialog);
    selector_label->setObjectName("statusStackName");
    auto* log_selector = new QComboBox(dialog);
    log_selector->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    selector_row->addWidget(selector_label);
    selector_row->addWidget(log_selector, 1);
    layout->addLayout(selector_row);

    auto* log_location_label = new QLabel(dialog);
    log_location_label->setObjectName("statusStackName");
    log_location_label->setWordWrap(true);
    log_location_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(log_location_label);

    auto* log_view = new QTextEdit(dialog);
    log_view->setReadOnly(true);
    log_view->setLineWrapMode(QTextEdit::NoWrap);
    log_view->setObjectName("logTextView");
    layout->addWidget(log_view, 1);

    auto* close_row = new QHBoxLayout();
    close_row->addStretch();
    auto* close_button = new QPushButton(QStringLiteral("关闭"), dialog);
    close_button->setObjectName("secondaryActionButton");
    close_button->setMinimumHeight(S(36));
    close_row->addWidget(close_button);
    layout->addLayout(close_row);

    dialog->setStyleSheet(QString(
        "QDialog { background: #0f141a; color: #e6edf7; }"
        "QLabel { color: #dbe7f1; font-size: %1px; font-weight: 600; }"
        "QComboBox { background: #181f29; color: #f4f8fc; border: 1px solid #354b60; border-radius: %2px; padding: %3px %4px; font-size: %1px; }"
        "QTextEdit#logTextView { background: #080d12; color: #dce7f1; border: 1px solid #27303d; border-radius: %5px; font-family: Consolas, 'Courier New'; font-size: %6px; }"
        "QPushButton { background: #2a4365; color: #ecf4fa; border: 1px solid #36546b; border-radius: %7px; padding: %8px %9px; font-size: %1px; font-weight: 600; }")
        .arg(S(13)).arg(S(6)).arg(S(6)).arg(S(8)).arg(S(8)).arg(S(12)).arg(S(7)).arg(S(6)).arg(S(12)));

    const QDir log_dir(QCoreApplication::applicationDirPath() + QStringLiteral("/logs"));
    const QFileInfoList logs = log_dir.entryInfoList({ QStringLiteral("*.log") },
                                                     QDir::Files | QDir::Readable,
                                                     QDir::Time);
    for (const QFileInfo& info : logs) {
        const QString label = QStringLiteral("%1  %2")
                                  .arg(info.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")))
                                  .arg(info.fileName());
        log_selector->addItem(label, info.absoluteFilePath());
    }

    const QStringList extra_log_files = {
        QString::fromLocal8Bit(qgetenv("TANKEYE_LOG_FILE")).trimmed()
    };
    const auto add_log_file = [log_selector](const QString& path) {
        const QFileInfo info(path);
        if (!info.exists() || !info.isFile()) {
            return;
        }
        for (int i = 0; i < log_selector->count(); ++i) {
            if (log_selector->itemData(i).toString() == info.absoluteFilePath()) {
                return;
            }
        }
        const QString label = QStringLiteral("%1  %2")
                                  .arg(info.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")))
                                  .arg(info.absoluteFilePath());
        log_selector->addItem(label, info.absoluteFilePath());
    };
    const auto add_log_dir = [add_log_file](const QString& path) {
        const QDir dir(path);
        const QFileInfoList logs = dir.entryInfoList({ QStringLiteral("*.log") },
                                                     QDir::Files,
                                                     QDir::Time);
        for (const QFileInfo& info : logs) {
            add_log_file(info.absoluteFilePath());
        }
    };
    for (const QString& path : extra_log_files) {
        if (!path.isEmpty()) {
            add_log_file(path);
        }
    }
    add_log_dir(QDir::current().filePath(QStringLiteral("logs")));
    log_location_label->setText(QStringLiteral("日志位置：%1；%2")
                                    .arg(log_dir.absolutePath(),
                                         QDir(QDir::current().filePath(QStringLiteral("logs"))).absolutePath()));

    const auto load_selected_log = [log_selector, log_view, log_location_label]() {
        const QString path = log_selector->currentData().toString();
        if (path.isEmpty()) {
            log_view->setPlainText(QStringLiteral("暂无日志。"));
            return;
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            log_view->setPlainText(QStringLiteral("日志打开失败：%1").arg(path));
            return;
        }
        log_view->setPlainText(QString::fromLocal8Bit(file.readAll()));
        log_view->moveCursor(QTextCursor::End);
    };

    connect(log_selector, QOverload<int>::of(&QComboBox::currentIndexChanged), dialog, [load_selected_log](int) {
        load_selected_log();
    });
    connect(close_button, &QPushButton::clicked, dialog, &QDialog::accept);

    load_selected_log();
    auto* auto_refresh_timer = new QTimer(dialog);
    auto_refresh_timer->setInterval(500);
    connect(auto_refresh_timer, &QTimer::timeout, dialog, [load_selected_log]() {
        load_selected_log();
    });
    auto_refresh_timer->start();
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}



void GraspMainWindow::applyEngineeringSettingsDraft(const EngineeringSettingsDraft& draft)
{
    grab_limits_ = draft.grab_limits;
    camera_ip_ = draft.camera_ip;
    camera_exposure_us_ = draft.camera_exposure_us;
    obb_conf_threshold_ = draft.obb_conf_threshold;
    obb_nms_threshold_ = draft.obb_nms_threshold;
    seg_conf_threshold_ = draft.seg_conf_threshold;
    seg_nms_threshold_ = draft.seg_nms_threshold;
    angle_offset_deg_ = draft.angle_offset_deg;
    center_ray_offset_px_ = draft.center_ray_offset_px;
    show_plc_center_debug_ = draft.show_plc_center_debug;
    show_head_ray_debug_ = draft.show_head_ray_debug;
    postprocess_debug_logging_enabled_ = draft.postprocess_debug_logging_enabled;
    angle_reverse_direction_ = draft.angle_reverse_direction;
    angle_range_mode_ = draft.angle_range_mode;
    axis_mapping_mode_ = draft.axis_mapping_mode;
    front_back_offset_ = draft.front_back_offset;
    left_right_offset_ = draft.left_right_offset;
    coordinate_transform_config_ = draft.coordinate_transform_config;
    rebuildCoordinateTransformState();

    workflow_.setCameraIp(camera_ip_.toStdString());
    workflow_.setCameraExposureUs(camera_exposure_us_);
    workflow_.setCenterRayOffsetPx(center_ray_offset_px_);
    workflow_.setPostprocessDebugLoggingEnabled(postprocess_debug_logging_enabled_);
    robot_controller_.setAngleCalibration(static_cast<float>(angle_offset_deg_),
                                          angle_reverse_direction_,
                                          angle_range_mode_);
    robot_controller_.setAxisMapping(axis_mapping_mode_);
    robot_controller_.setAxisCompensation(static_cast<float>(front_back_offset_),
                                          static_cast<float>(left_right_offset_));
}

void GraspMainWindow::loadImage()
{
    if (image_load_watcher_ && !image_load_watcher_->isFinished()) {
        updateStatusMessage(QStringLiteral("图片正在加载，请稍候..."), 2000);
        return;
    }

    const QString path = QFileDialog::getOpenFileName(this,
                                                      QStringLiteral("选择图片"),
                                                      currentImagePath(),
                                                      QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp);;All Files (*)"));
    if (path.isEmpty()) {
        return;
    }

    workflow_.stopCamera();
    input_mode_ = InputMode::Image;
    updateStatusMessage(QStringLiteral("正在加载图片..."));
    if (load_image_button_) {
        load_image_button_->setEnabled(false);
    }
    refreshDeviceStatus();
    refreshRuntimeStrip();

    auto* watcher = new QFutureWatcher<ImageLoadResult>(this);
    image_load_watcher_ = watcher;

    connect(watcher, &QFutureWatcher<ImageLoadResult>::finished, this, [this, watcher]() {
        const ImageLoadResult result = watcher->result();
        if (image_load_watcher_ == watcher) {
            image_load_watcher_ = nullptr;
        }
        watcher->deleteLater();

        if (load_image_button_) {
            load_image_button_->setEnabled(true);
        }

        if (!result.error_message.isEmpty() || result.image.empty()) {
            cout << "[Image] load failed: " << result.path.toStdString() << endl;
            QMessageBox::critical(this, QStringLiteral("读取失败"), QStringLiteral("无法读取所选图片。"));
            refreshDeviceStatus();
            refreshRuntimeStrip();
            updateStatusMessage(QStringLiteral("图片加载失败。"), 5000);
            return;
        }

        current_frame_ = result.image;
        current_image_path_ = QFileInfo(result.path).absoluteFilePath();
        image_path_label_->setText(QFileInfo(result.path).fileName());
        image_path_label_->setToolTip(current_image_path_);
        cout << "[Image] loaded: " << result.path.toStdString()
             << " (" << current_frame_.cols << "x" << current_frame_.rows << ")" << endl;
        clearResults();
        renderCurrentFrame();
        refreshDeviceStatus();
        refreshRuntimeStrip();
        updateStatusMessage(QStringLiteral("图片已加载，点击开始检测执行视觉检测。"));
    });

    watcher->setFuture(QtConcurrent::run([path]() {
        ImageLoadResult result;
        result.path = path;
        if (path.trimmed().isEmpty()) {
            result.error_message = QStringLiteral("empty path");
            return result;
        }

        result.image = imread(path.toStdString());
        if (result.image.empty()) {
            result.error_message = QStringLiteral("read failed");
        }
        return result;
    }));
}

void GraspMainWindow::openCamera()
{
    if (plc_runtime_state_.active()) {
        updateStatusMessage(QStringLiteral("抓取运行中，相机已在采集。"), 5000);
        return;
    }

    workflow_.stopCamera();
    clearResults();

    if (!workflow_.areModelsLoaded()) {
        QMessageBox::warning(this,
                             QStringLiteral("模型未加载"),
                             QStringLiteral("请先在工程设置中加载模型。"));
        refreshDeviceStatus();
        refreshRuntimeStrip();
        return;
    }

    input_mode_ = InputMode::Camera;
    resetCameraUiCounters();
    setEmptyPreviewMessage(QStringLiteral("正在打开默认相机..."));
    const bool started = workflow_.startFrameCapture(
        [this](const Mat& frame) {
            const int seen = ++camera_ui_seen_count_;
            bool expected = false;
            if (!camera_ui_update_pending_.compare_exchange_strong(expected, true)) {
                const int skipped = ++camera_ui_skipped_count_;
                if (seen == 1 || seen % 120 == 0) {
                    cout << "[CameraCaptureUI] seen=" << seen
                         << " skipped=" << skipped
                         << " pending=1"
                         << endl;
                }
                return;
            }

            const auto queued_at = std::chrono::steady_clock::now();
            Mat frame_copy = frame.clone();
            QMetaObject::invokeMethod(
                this,
                [this, frame_copy, queued_at]() {
                    const auto ui_start = std::chrono::steady_clock::now();
                    const int rendered = camera_ui_rendered_count_.load() + 1;
                    current_frame_ = frame_copy;
                    image_path_label_->setText(QStringLiteral("相机采集中"));
                    renderCurrentFrame();
                    refreshDeviceStatus();
                    refreshRuntimeStrip();
                    const auto ui_end = std::chrono::steady_clock::now();
                    camera_ui_rendered_count_ = rendered;
                    if (rendered == 1 || rendered % 120 == 0) {
                        cout << "[CameraCaptureUI] rendered=" << rendered
                             << " seen=" << camera_ui_seen_count_.load()
                             << " skipped=" << camera_ui_skipped_count_.load()
                             << " queue_ms=" << MsSince(queued_at, ui_start)
                             << " ui_update_ms=" << MsSince(ui_start, ui_end)
                             << " image=" << frame_copy.cols << "x" << frame_copy.rows
                             << endl;
                    }
                    if (rendered == 1) {
                        updateStatusMessage(QStringLiteral("相机已打开，当前仅采集画面；点击开始检测可手动检测一次。"), 5000);
                    }
                    camera_ui_update_pending_ = false;
                },
                Qt::QueuedConnection);
        },
        [this](const string& error_message) {
            QMetaObject::invokeMethod(
                this,
                [this, error_message]() {
                    camera_ui_update_pending_ = false;
                    input_mode_ = InputMode::Idle;
                    refreshDeviceStatus();
                    refreshRuntimeStrip();
                    QMessageBox::critical(this, QStringLiteral("相机错误"), QString::fromStdString(error_message));
                    updateStatusMessage(QStringLiteral("相机采集已停止。"));
                },
                Qt::QueuedConnection);
        });

    refreshDeviceStatus();
    refreshRuntimeStrip();
    if (started) {
        updateStatusMessage(QStringLiteral("默认相机已启动，当前只采集画面，不自动检测。"));
    }
}

void GraspMainWindow::closeCamera()
{
    if (plc_runtime_state_.active()) {
        updateStatusMessage(QStringLiteral("抓取运行中，请使用“停止抓取”退出 PLC 抓取。"), 5000);
        return;
    }

    if (!workflow_.isCameraRunning()) {
        updateStatusMessage(QStringLiteral("相机未打开。"), 3000);
        refreshDeviceStatus();
        refreshRuntimeStrip();
        return;
    }

    workflow_.stopCamera();
    camera_ui_update_pending_ = false;
    if (input_mode_ == InputMode::Camera) {
        input_mode_ = current_frame_.empty() ? InputMode::Idle : InputMode::Image;
    }
    image_path_label_->setText(QStringLiteral("相机已关闭"));
    refreshDeviceStatus();
    refreshRuntimeStrip();
    updateStatusMessage(QStringLiteral("相机已关闭。"), 4000);
}

void GraspMainWindow::startDetection()
{
    if (image_detection_running_) {
        updateStatusMessage(QStringLiteral("图片检测中，请稍候..."), 2000);
        return;
    }

    if (!workflow_.areModelsLoaded()) {
        QMessageBox::warning(this,
                             QStringLiteral("模型未加载"),
                             QStringLiteral("请先在工程设置中加载模型。"));
        return;
    }

    if (input_mode_ == InputMode::Camera) {
        if (current_frame_.empty()) {
            if (!workflow_.isCameraRunning()) {
                openCamera();
            } else {
                updateStatusMessage(QStringLiteral("相机采集中，等待第一帧..."), 2000);
            }
            return;
        }
    } else if (current_frame_.empty()) {
        QMessageBox::information(this, QStringLiteral("没有输入"), QStringLiteral("请先加载图片，或打开相机。"));
        return;
    }

    image_detection_running_ = true;
    refreshDeviceStatus();
    refreshRuntimeStrip();
    updateStatusMessage(QStringLiteral("图片检测中..."));

    const auto frame = std::make_shared<Mat>(current_frame_.clone());
    const auto result = std::make_shared<FrameInferenceResult>();
    const GrabLimitConfig limits = grab_limits_;
    const auto image_detect_start = std::chrono::steady_clock::now();
    auto* watcher = new QFutureWatcher<QString>(this);
    image_detection_watcher_ = watcher;

    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, frame, result, image_detect_start]() {
        const auto finished_start = std::chrono::steady_clock::now();
        const QString error_message = watcher->result();
        if (image_detection_watcher_ == watcher) {
            image_detection_watcher_ = nullptr;
        }
        watcher->deleteLater();
        image_detection_running_ = false;

        if (!error_message.isEmpty()) {
            refreshDeviceStatus();
            refreshRuntimeStrip();
            updateStatusMessage(QStringLiteral("图片检测失败。"), 5000);
            QMessageBox::critical(this, QStringLiteral("检测失败"), error_message);
            return;
        }

        const auto ui_start = std::chrono::steady_clock::now();
        setFrameAndResult(*frame, *result);
        const auto ui_end = std::chrono::steady_clock::now();
        cout << "[ImagePerf] wall_to_finished_ms=" << MsSince(image_detect_start, finished_start)
             << " ui_update_ms=" << MsSince(ui_start, ui_end)
             << " wall_to_display_ms=" << MsSince(image_detect_start, ui_end)
             << " image=" << frame->cols << "x" << frame->rows
             << " total_process_ms=" << result->total_inference_ms
             << endl;
        refreshDeviceStatus();
        refreshRuntimeStrip();
        updateStatusMessage(QStringLiteral("图片检测完成。"), 3000);
        const FrameInferenceResult result_for_plc = *result;
        QTimer::singleShot(0, this, [this, result_for_plc]() {
            writeCurrentResultToPlc(QStringLiteral("图片联合检测"), false, result_for_plc);
        });
    });

    const CoordinateTransformState coordinate_state = coordinate_transform_state_;
    const PlcOutputConfig plc_config = plcOutputConfig();
    watcher->setFuture(QtConcurrent::run([this, frame, result, coordinate_state, limits, plc_config]() -> QString {
        const FrameProcessingResult process_result =
            ProcessVisionFrame(workflow_,
                               *frame,
                               limits,
                               coordinate_state,
                               plc_config.axis_mapping_mode,
                               false);
        *result = process_result.frame_result;
        return process_result.error_message;
    }));
}
void GraspMainWindow::stopDetection()
{
    if (plc_runtime_state_.workflowState() == RuntimeWorkflowState::PlcPolling ||
        plc_runtime_state_.workflowState() == RuntimeWorkflowState::PlcWriting) {
        stopPlcLinkState(RuntimeWorkflowState::Stopped);
    } else {
        stopPlcLinkState(plc_runtime_state_.workflowState());
    }
    workflow_.stopCamera();
    camera_ui_update_pending_ = false;
    refreshDeviceStatus();
    refreshRuntimeStrip();
    updateStatusMessage(QStringLiteral("检测已停止。"), 4000);
}

void GraspMainWindow::togglePlcLinkMode()
{
    if (plc_runtime_state_.active()) {
        stopPlcLinkState(RuntimeWorkflowState::Idle);
        refreshDeviceStatus();
        refreshRuntimeStrip();
        updateStatusMessage(QStringLiteral("抓取已停止。"), 4000);
        return;
    }

    if (!workflow_.areModelsLoaded()) {
        QMessageBox::warning(this,
                             QStringLiteral("模型未加载"),
                             QStringLiteral("请先在工程设置中加载模型。"));
        return;
    }

    if (workflow_.isCameraRunning()) {
        workflow_.stopCamera();
        camera_ui_update_pending_ = false;
    }

    if (!workflow_.isCameraRunning()) {
        input_mode_ = InputMode::Camera;
        resetCameraUiCounters();
        clearResults();
        setEmptyPreviewMessage(QStringLiteral("相机正在打开，等待接收信号。"));

        const bool started = workflow_.startFrameCapture(
            [this](const Mat& frame) {
                const int seen = ++camera_ui_seen_count_;
                bool expected = false;
                if (!camera_ui_update_pending_.compare_exchange_strong(expected, true)) {
                    ++camera_ui_skipped_count_;
                    return;
                }

                Mat frame_copy = frame.clone();
                QMetaObject::invokeMethod(
                    this,
                    [this, frame_copy, seen]() {
                        current_frame_ = frame_copy;
                        image_path_label_->setText(QStringLiteral("PLC触发采集中"));
                        refreshDeviceStatus();
                        refreshRuntimeStrip();
                        if (seen == 1 || seen % 120 == 0) {
                            cout << "[CameraCaptureUI] cached=" << seen
                                 << " seen=" << seen
                                 << " skipped=" << camera_ui_skipped_count_.load()
                                 << " image=" << frame_copy.cols << "x" << frame_copy.rows
                                 << endl;
                        }
                        camera_ui_update_pending_ = false;
                    },
                    Qt::QueuedConnection);
            },
            [this](const string& error_message) {
                QMetaObject::invokeMethod(
                    this,
                    [this, error_message]() {
                        camera_ui_update_pending_ = false;
                        stopPlcLinkState(RuntimeWorkflowState::Idle);
                        input_mode_ = InputMode::Idle;
                        refreshDeviceStatus();
                        refreshRuntimeStrip();
                        QMessageBox::critical(this, QStringLiteral("相机错误"), QString::fromStdString(error_message));
                        updateStatusMessage(QStringLiteral("抓取已停止：相机采集失败。"), 5000);
                    },
                    Qt::QueuedConnection);
            });

        if (!started || !workflow_.isCameraRunning()) {
            stopPlcLinkState(RuntimeWorkflowState::Idle);
            refreshDeviceStatus();
            refreshRuntimeStrip();
            updateStatusMessage(QStringLiteral("抓取未启动：相机未成功打开。"), 4000);
            return;
        }
    }

    startPlcPollingState();
    refreshDeviceStatus();
    refreshRuntimeStrip();
    updateStatusMessage(QStringLiteral("抓取已启动，等待 D1500=1。"), 5000);
}

void GraspMainWindow::pollPlcTrigger()
{
    if (!plc_runtime_state_.active() ||
        plc_runtime_state_.pollBusy() ||
        plc_runtime_state_.waitingForFrame() ||
        (plc_poll_watcher_ && !plc_poll_watcher_->isFinished())) {
        return;
    }

    plc_runtime_state_.setPollBusy(true);
    const auto triggered = std::make_shared<bool>(false);
    auto* watcher = new QFutureWatcher<QString>(this);
    plc_poll_watcher_ = watcher;

    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, triggered]() {
        finishPlcTriggerPoll(watcher, triggered);
    });

    watcher->setFuture(QtConcurrent::run([this, triggered]() -> QString {
        const PlcTriggerReadResult read_result = ReadPlcPhotoTrigger(robot_controller_);
        *triggered = read_result.triggered;
        return read_result.error_message;
    }));
}

void GraspMainWindow::finishPlcTriggerPoll(QFutureWatcher<QString>* watcher,
                                           const std::shared_ptr<bool>& triggered)
{
    const QString error_message = watcher->result();
    if (plc_poll_watcher_ == watcher) {
        plc_poll_watcher_ = nullptr;
    }
    watcher->deleteLater();

    if (!error_message.isEmpty()) {
        plc_runtime_state_.setPollBusy(false);
        updateStatusMessage(QStringLiteral("PLC read failed: %1").arg(error_message), 3000);
        return;
    }

    if (!*triggered) {
        plc_runtime_state_.setPollBusy(false);
        return;
    }

    plc_runtime_state_.beginWriting();
    refreshRuntimeStrip();
    processPlcTriggeredFrame();
}

void GraspMainWindow::processPlcTriggeredFrame()
{
    if (!workflow_.areModelsLoaded()) {
        plc_runtime_state_.finishWaitingForFrame();
        plc_runtime_state_.restoreIdleOrPolling();
        refreshRuntimeStrip();
        updateStatusMessage(QStringLiteral("PLC触发被忽略：模型未加载。"), 3000);
        return;
    }

    if (current_frame_.empty()) {
        if (workflow_.isCameraRunning()) {
            if (!plc_runtime_state_.waitingForFrame()) {
                plc_runtime_state_.beginWaitingForFrame();
                cout << "[PLCPerf] trigger received before first camera frame; waiting for frame" << endl;
                refreshRuntimeStrip();
                updateStatusMessage(QStringLiteral("已收到 PLC 触发，等待相机第一帧。"), 2000);
            }
            QTimer::singleShot(50, this, [this]() {
                if (!plc_runtime_state_.active() || !plc_runtime_state_.waitingForFrame()) {
                    return;
                }
                processPlcTriggeredFrame();
            });
            return;
        }

        const auto plc_start = std::chrono::steady_clock::now();
        const PlcTriggerProcessResult missing_frame_result =
            ProcessPlcTriggeredMissingFrame(robot_controller_, plcOutputConfig(), plc_start);
        plc_runtime_state_.finishWaitingForFrame();
        if (!missing_frame_result.error_message.isEmpty()) {
            plc_runtime_state_.restoreIdleOrPolling();
            refreshRuntimeStrip();
            updateStatusMessage(QStringLiteral("PLC trigger failed: %1").arg(missing_frame_result.error_message), 5000);
            return;
        }
        const int pick_status_register = robot_controller_.plcRegisterMap().pick_status;
        last_plc_reject_reason_ = missing_frame_result.reject_reason;
        plc_runtime_state_.completeMissingFrame(pick_status_register);
        refreshRuntimeStrip();
        updateStatusMessage(QStringLiteral("PLC触发时没有相机帧，已写 D%1=3。拒绝原因：%2")
                                .arg(pick_status_register)
                                .arg(last_plc_reject_reason_),
                            8000);
        if (plc_test_result_dialog_pending_) {
            plc_test_result_dialog_pending_ = false;
            showPlcTestResultDialog(missing_frame_result.frame_result);
        }
        return;
    }

    plc_runtime_state_.beginWriting();
    refreshRuntimeStrip();

    const auto plc_start = std::chrono::steady_clock::now();
    const auto frame = std::make_shared<Mat>(current_frame_.clone());
    const auto result = std::make_shared<FrameInferenceResult>();
    const auto plc_process_result = std::make_shared<PlcTriggerProcessResult>();
    const GrabLimitConfig limits = grab_limits_;
    auto* watcher = new QFutureWatcher<QString>(this);
    plc_detection_watcher_ = watcher;

    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, frame, result, plc_process_result, plc_start]() {
        finishPlcTriggeredFrame(watcher, frame, result, plc_process_result, plc_start);
    });

    const CoordinateTransformState coordinate_state = coordinate_transform_state_;
    const PlcOutputConfig plc_config = plcOutputConfig();
    watcher->setFuture(QtConcurrent::run([this, frame, result, plc_process_result, plc_start, limits, coordinate_state, plc_config]() -> QString {
        *plc_process_result =
            ProcessPlcTriggeredFrame(workflow_,
                                     robot_controller_,
                                     *frame,
                                     limits,
                                     coordinate_state,
                                     plc_config,
                                     plc_start);
        *result = plc_process_result->frame_result;
        return plc_process_result->error_message;
    }));
}

void GraspMainWindow::finishPlcTriggeredFrame(QFutureWatcher<QString>* watcher,
                                              const std::shared_ptr<Mat>& frame,
                                              const std::shared_ptr<FrameInferenceResult>& result,
                                              const std::shared_ptr<PlcTriggerProcessResult>& plc_process_result,
                                              const std::chrono::steady_clock::time_point& plc_start)
{
    const QString error_message = watcher->result();
    if (plc_detection_watcher_ == watcher) {
        plc_detection_watcher_ = nullptr;
    }
    watcher->deleteLater();
    plc_runtime_state_.setPollBusy(false);

    if (!error_message.isEmpty()) {
        plc_runtime_state_.fail(error_message);
        refreshDeviceStatus();
        refreshRuntimeStrip();
        updateStatusMessage(QStringLiteral("PLC trigger failed: %1").arg(error_message), 6000);
        return;
    }

    const auto ui_start = std::chrono::steady_clock::now();
    setFrameAndResult(*frame, *result);
    const auto ui_end = std::chrono::steady_clock::now();
    cout << "[PLCPerf] ui_update_ms=" << MsSince(ui_start, ui_end)
         << " trigger_to_display_ms=" << MsSince(plc_start, ui_end)
         << endl;
    const double total_ms = MsSince(plc_start, ui_end);
    const int pick_status_register = robot_controller_.plcRegisterMap().pick_status;
    if (plc_process_result && !plc_process_result->reject_reason.isEmpty()) {
        last_plc_reject_reason_ = plc_process_result->reject_reason;
        plc_runtime_state_.reject(last_plc_reject_reason_,
                                  pick_status_register,
                                  result->pick_status_code,
                                  total_ms);
        updateStatusMessage(QStringLiteral("PLC拒绝：%1").arg(last_plc_reject_reason_), 8000);
    } else {
        plc_runtime_state_.completeFrame(*result, pick_status_register, total_ms);
    }
    refreshDeviceStatus();
    refreshRuntimeStrip();
    if (plc_test_result_dialog_pending_) {
        plc_test_result_dialog_pending_ = false;
        showPlcTestResultDialog(*result);
    }
}

void GraspMainWindow::writeCurrentResultToPlc(const QString& context,
                                              bool clear_trigger,
                                              const FrameInferenceResult& result)
{
    const GrabLimitConfig limits = grab_limits_;
    const CoordinateTransformState coordinate_state = coordinate_transform_state_;
    auto* watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, context]() {
        const QString error_message = watcher->result();
        watcher->deleteLater();
        if (!error_message.isEmpty()) {
            updateStatusMessage(QStringLiteral("Image detection complete, but PLC write failed: %1")
                                    .arg(error_message),
                                6000);
            return;
        }
        updateStatusMessage(QStringLiteral("Image detection complete, PLC written."), 5000);
        return;
        if (!error_message.isEmpty()) {
            updateStatusMessage(QStringLiteral("%1完成，但 PLC 写入失败：%2")
                                    .arg(context, error_message),
                                6000);
            return;
        }

        updateStatusMessage(QStringLiteral("%1完成，已写入 PLC。").arg(context), 5000);
    });

    const PlcOutputConfig plc_config = plcOutputConfig();
    watcher->setFuture(QtConcurrent::run([this, result, clear_trigger, limits, coordinate_state, plc_config]() -> QString {
        const PlcManualWriteResult write_result =
            WriteFrameResultToPlc(robot_controller_,
                                  result,
                                  limits,
                                  coordinate_state,
                                  plc_config,
                                  clear_trigger);
        return write_result.error_message;
    }));
}

void GraspMainWindow::showPlcTestResultDialog(const FrameInferenceResult& result)
{
    const PlcWriteResult write_result = BuildPlcWriteResult(result, plcOutputConfig());
    const QString message = QStringLiteral("测试成功，结果已写回。\n\n%1")
                                .arg(FormatPlcWriteResult(write_result, robot_controller_.plcRegisterMap()));
    const PlcRegisterMap registers = robot_controller_.plcRegisterMap();
    updateStatusMessage(QStringLiteral("PLC test succeeded: D%1=%2, D%3=%4")
                            .arg(registers.pick_status)
                            .arg(FormatNumber(write_result.pick_status))
                            .arg(registers.head_type)
                            .arg(FormatNumber(write_result.head_type)),
                        6000);
    QMessageBox::information(this, QStringLiteral("测试成功"), message);
}

void GraspMainWindow::writePlcTestValues()
{
    if (plc_test_watcher_ && !plc_test_watcher_->isFinished()) {
        updateStatusMessage(QStringLiteral("PLC test is already running."), 3000);
        return;
    }

    auto* watcher = new QFutureWatcher<QString>(this);
    plc_test_watcher_ = watcher;
    plc_test_button_->setEnabled(false);
    updateStatusMessage(QStringLiteral("PLC test is sending..."), 3000);

    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher]() {
        const QString error_message = watcher->result();
        if (plc_test_watcher_ == watcher) {
            plc_test_watcher_ = nullptr;
        }
        watcher->deleteLater();
        refreshDeviceStatus();

        if (error_message.isEmpty()) {
            if (robot_controller_.simulationEnabled() && plc_runtime_state_.active()) {
                plc_test_result_dialog_pending_ = true;
                updateStatusMessage(QStringLiteral("PLC test sent. Waiting for result..."), 5000);
            } else if (robot_controller_.simulationEnabled()) {
                updateStatusMessage(QStringLiteral("PLC test sent, but PLC link is not active."), 5000);
                QMessageBox::information(this,
                                         QStringLiteral("测试"),
                                         QStringLiteral("模拟触发已发送。请先点击“抓取”，再点击“测试”，才能看到检测返回结果。"));
            } else {
                const PlcWriteResult test_result = BuildDiagnosticPlcWriteResult(plcOutputConfig());
                const PlcRegisterMap registers = robot_controller_.plcRegisterMap();
                const QString message = QStringLiteral("测试成功，诊断值已写入。\n\n"
                                                      "D%1 前后：%2\n"
                                                      "D%3 左右：%4\n"
                                                      "D%5 角度：%6\n"
                                                      "D%7 状态：1.0\n"
                                                      "D%8 类型：4.0")
                                           .arg(registers.front_back)
                                           .arg(FormatNumber(test_result.x))
                                           .arg(registers.left_right)
                                           .arg(FormatNumber(test_result.y))
                                           .arg(registers.angle)
                                           .arg(FormatNumber(test_result.angle))
                                           .arg(registers.pick_status)
                                           .arg(registers.head_type);
                updateStatusMessage(QStringLiteral("PLC test succeeded."), 5000);
                QMessageBox::information(this, QStringLiteral("测试成功"), message);
            }
        } else {
            updateStatusMessage(QStringLiteral("PLC test failed: %1").arg(error_message), 6000);
            QMessageBox::warning(this,
                                 QStringLiteral("测试失败"),
                                 QStringLiteral("测试失败：%1").arg(error_message));
        }
    });

    watcher->setFuture(QtConcurrent::run([this]() -> QString {
        std::string error_message;
        if (robot_controller_.writePlcTestValues(&error_message)) {
            return QString();
        }
        return QString::fromStdString(error_message);
    }));
}

void GraspMainWindow::loadLimitSettings()
{
    grab_limits_ = EngineeringSettingsService::LoadGrabLimits(app_config_.machine_limits);
}

void GraspMainWindow::loadCameraSettings()
{
    const CameraSettings camera = EngineeringSettingsService::LoadCameraSettings(app_config_.camera);
    camera_ip_ = camera.ip;
    camera_exposure_us_ = camera.exposure_us;
    if (camera_ip_.isEmpty()) {
        camera_ip_ = QString::fromLocal8Bit(qgetenv("TANKEYE_CAMERA_IP")).trimmed();
    }
    workflow_.setCameraIp(camera_ip_.toStdString());
    workflow_.setCameraExposureUs(camera_exposure_us_);
}

void GraspMainWindow::loadModelThresholdSettings()
{
    const ModelThresholdSettings thresholds = EngineeringSettingsService::LoadModelThresholdSettings();
    obb_conf_threshold_ = thresholds.obb_conf_threshold;
    obb_nms_threshold_ = thresholds.obb_nms_threshold;
    seg_conf_threshold_ = thresholds.seg_conf_threshold;
    seg_nms_threshold_ = thresholds.seg_nms_threshold;
}

void GraspMainWindow::loadAngleCalibrationSettings()
{
    const AngleCalibrationSettings angle =
        EngineeringSettingsService::LoadAngleCalibrationSettings(app_config_.angle_calibration);
    angle_offset_deg_ = angle.offset_deg;
    angle_reverse_direction_ = angle.reverse_direction;
    angle_range_mode_ = angle.range_mode;

    robot_controller_.setAngleCalibration(static_cast<float>(angle_offset_deg_),
                                          angle_reverse_direction_,
                                          angle_range_mode_);
}

void GraspMainWindow::loadObbPostprocessSettings()
{
    const ObbPostprocessSettings postprocess = EngineeringSettingsService::LoadObbPostprocessSettings();
    center_ray_offset_px_ = postprocess.center_ray_offset_px;
    show_plc_center_debug_ = postprocess.show_plc_center_debug;
    show_head_ray_debug_ = postprocess.show_head_ray_debug;
    postprocess_debug_logging_enabled_ = postprocess.debug_logging_enabled;
    workflow_.setCenterRayOffsetPx(center_ray_offset_px_);
    workflow_.setPostprocessDebugLoggingEnabled(postprocess_debug_logging_enabled_);
}

void GraspMainWindow::loadAxisMappingSettings()
{
    axis_mapping_mode_ = EngineeringSettingsService::LoadAxisMappingMode(app_config_.axis_mapping_mode);
    robot_controller_.setAxisMapping(axis_mapping_mode_);
}

void GraspMainWindow::loadAxisCompensationSettings()
{
    const AxisCompensationSettings compensation =
        EngineeringSettingsService::LoadAxisCompensationSettings(app_config_.axis_compensation);
    front_back_offset_ = compensation.front_back_offset;
    left_right_offset_ = compensation.left_right_offset;

    robot_controller_.setAxisCompensation(static_cast<float>(front_back_offset_),
                                          static_cast<float>(left_right_offset_));
}

void GraspMainWindow::loadCoordinateTransformSettings()
{
    coordinate_transform_config_ = EngineeringSettingsService::LoadCoordinateTransformSettings();
    rebuildCoordinateTransformState();
}

void GraspMainWindow::saveCameraSettings() const
{
    EngineeringSettingsService::SaveCameraSettings({ camera_ip_, camera_exposure_us_ });
}

void GraspMainWindow::saveModelThresholdSettings() const
{
    EngineeringSettingsService::SaveModelThresholdSettings({
        obb_conf_threshold_,
        obb_nms_threshold_,
        seg_conf_threshold_,
        seg_nms_threshold_,
    });
}

void GraspMainWindow::saveAngleCalibrationSettings() const
{
    EngineeringSettingsService::SaveAngleCalibrationSettings({
        angle_offset_deg_,
        angle_reverse_direction_,
        angle_range_mode_,
    });
}

void GraspMainWindow::saveObbPostprocessSettings() const
{
    EngineeringSettingsService::SaveObbPostprocessSettings({
        center_ray_offset_px_,
        show_plc_center_debug_,
        show_head_ray_debug_,
        postprocess_debug_logging_enabled_,
    });
}

void GraspMainWindow::saveAxisMappingSettings() const
{
    EngineeringSettingsService::SaveAxisMappingMode(axis_mapping_mode_);
}

void GraspMainWindow::saveAxisCompensationSettings() const
{
    EngineeringSettingsService::SaveAxisCompensationSettings({
        front_back_offset_,
        left_right_offset_,
    });
}

bool GraspMainWindow::saveCoordinateTransformSettings(QString* error_message) const
{
    return EngineeringSettingsService::SaveCoordinateTransformSettings(coordinate_transform_config_,
                                                                       error_message);
}

void GraspMainWindow::rebuildCoordinateTransformState()
{
    coordinate_transform_state_ = BuildCoordinateTransformState(coordinate_transform_config_);
}

void GraspMainWindow::saveLimitSettings() const
{
    EngineeringSettingsService::SaveGrabLimits(grab_limits_);
}

float GraspMainWindow::calibratedAngle(float angle_deg) const
{
    return ApplyPlcAngleCalibration(angle_deg, plcOutputConfig());
}

PlcOutputConfig GraspMainWindow::plcOutputConfig() const
{
    PlcOutputConfig config;
    config.angle_offset_deg = static_cast<float>(angle_offset_deg_);
    config.angle_reverse_direction = angle_reverse_direction_;
    config.angle_range_mode = angle_range_mode_;
    config.axis_mapping_mode = axis_mapping_mode_;
    config.front_back_offset = static_cast<float>(front_back_offset_);
    config.left_right_offset = static_cast<float>(left_right_offset_);
    return config;
}

QString GraspMainWindow::frontBackAxisLabel() const
{
    return axis_mapping_mode_ == AxisMappingMode::FrontBackMachineX
        ? QStringLiteral("前后 / 机械X")
        : QStringLiteral("前后 / 机械Y");
}

QString GraspMainWindow::leftRightAxisLabel() const
{
    return axis_mapping_mode_ == AxisMappingMode::FrontBackMachineX
        ? QStringLiteral("左右 / 机械Y")
        : QStringLiteral("左右 / 机械X");
}

void GraspMainWindow::setFrameAndResult(const Mat& frame, const FrameInferenceResult& result)
{
    current_frame_ = frame.clone();
    current_result_ = result;

    renderCurrentFrame();
    refreshInfoPanel();
    refreshTargetTable();
    refreshDeviceStatus();
    refreshRuntimeStrip();
}

bool GraspMainWindow::loadImageFromPath(const QString& path, bool notify)
{
    if (path.trimmed().isEmpty()) {
        cout << "[Image] load rejected: empty path" << endl;
        return false;
    }

    workflow_.stopCamera();
    input_mode_ = InputMode::Image;

    Mat image = imread(path.toStdString());
    if (image.empty()) {
        cout << "[Image] load failed: " << path.toStdString() << endl;
        if (notify) {
            QMessageBox::critical(this, QStringLiteral("读取失败"), QStringLiteral("无法读取所选图片。"));
        }
        return false;
    }

    current_frame_ = image;
    current_image_path_ = QFileInfo(path).absoluteFilePath();
    image_path_label_->setText(QFileInfo(path).fileName());
    image_path_label_->setToolTip(current_image_path_);
    cout << "[Image] loaded: " << path.toStdString()
         << " (" << image.cols << "x" << image.rows << ")" << endl;
    clearResults();
    renderCurrentFrame();
    refreshDeviceStatus();
    refreshRuntimeStrip();
    if (notify) {
        updateStatusMessage(QStringLiteral("图片已加载，点击开始检测执行视觉检测。"));
    }
    return true;
}

void GraspMainWindow::renderCurrentFrame()
{
    if (current_frame_.empty()) {
        return;
    }

    const QSize viewport_size = image_label_->size();
    if (viewport_size.width() <= 0 || viewport_size.height() <= 0) {
        return;
    }
    const QSize display_qsize = FitImageSize(current_frame_.cols,
                                             current_frame_.rows,
                                             viewport_size);
    Mat display;
    if (display_qsize.width() != current_frame_.cols || display_qsize.height() != current_frame_.rows) {
        cv::resize(current_frame_,
                   display,
                   Size(display_qsize.width(), display_qsize.height()),
                   0.0,
                   0.0,
                   display_qsize.width() < current_frame_.cols ? INTER_AREA : INTER_LINEAR);
    } else {
        display = current_frame_.clone();
    }

    const double scale_x = display.cols / static_cast<double>(current_frame_.cols);
    const double scale_y = display.rows / static_cast<double>(current_frame_.rows);
    const FrameInferenceResult display_result = ScaleResultForDisplay(current_result_,
                                                                      scale_x,
                                                                      scale_y,
                                                                      display.size(),
                                                                      display.size(),
                                                                      0,
                                                                      0,
                                                                      show_all_detections_);
    DrawFrameOverlay(display,
                     display_result,
                     -1,
                     false,
                     show_all_detections_,
                     show_plc_center_debug_,
                     show_head_ray_debug_);

    const QImage image = MatToQImage(display);
    if (image.isNull()) {
        image_label_->setText(QStringLiteral("图像渲染失败"));
        return;
    }

    const QPixmap pixmap = QPixmap::fromImage(image);
    image_label_->setPixmap(pixmap);
}

void GraspMainWindow::refreshInfoPanel()
{
    const int resolved_index = current_result_.primary_index;

    if (resolved_index < 0 || resolved_index >= static_cast<int>(current_result_.detections.size())) {
        SetValueText(machine_x_value_label_, QStringLiteral("--"));
        SetValueText(machine_y_value_label_, QStringLiteral("--"));
        SetValueText(angle_value_label_, QStringLiteral("--"));
        SetValueText(pick_status_value_label_, QString::number(current_result_.pick_status_code));
        SetValueText(head_type_value_label_,
                     current_result_.head_type_code > 0
                         ? QStringLiteral("%1 %2")
                               .arg(current_result_.head_type_code)
                               .arg(QString::fromStdString(current_result_.head_type_text))
                         : QStringLiteral("--"));
        return;
    }

    const auto& detection = current_result_.detections[resolved_index];
    SetValueText(machine_x_value_label_, FormatPrimaryCoordinate(detection, true));
    SetValueText(machine_y_value_label_, FormatPrimaryCoordinate(detection, false));
    SetValueText(angle_value_label_, FormatNumber(calibratedAngle(detection.angle_deg)) + QStringLiteral(" deg"));
    SetValueText(pick_status_value_label_, QString::number(detection.pick_status_code));
    SetValueText(head_type_value_label_,
                 detection.head_type_code > 0
                     ? QStringLiteral("%1 %2")
                           .arg(detection.head_type_code)
                           .arg(QString::fromStdString(detection.head_type_text))
                     : QStringLiteral("--"));
}

QFrame* GraspMainWindow::createTargetCard(QLabel*& title_label,
                                          QLabel*& image_x_label,
                                          QLabel*& image_y_label,
                                          QLabel*& machine_x_label,
                                          QLabel*& machine_y_label,
                                          QLabel*& angle_label,
                                          QLabel*& pick_status_label,
                                          QLabel*& head_type_label)
{
    const PlcRegisterMap registers = robot_controller_.plcRegisterMap();
    auto* card = new QFrame(target_list_content_);
    card->setObjectName("infoCard");

    auto* card_layout = new QVBoxLayout(card);
    card_layout->setContentsMargins(SM(12, 12, 12, 12));
    card_layout->setSpacing(S(8));

    title_label = new QLabel(card);
    title_label->setObjectName("statusStackName");
    card_layout->addWidget(title_label);

    auto* metrics_layout = new QHBoxLayout();
    metrics_layout->setSpacing(S(8));

    auto create_metric = [card, metrics_layout](const QString& name, QLabel*& value_label) {
        auto* metric_card = new QFrame(card);
        metric_card->setObjectName("compactMetricCell");

        auto* metric_layout = new QVBoxLayout(metric_card);
        metric_layout->setContentsMargins(SM(10, 8, 10, 8));
        metric_layout->setSpacing(S(3));

        auto* name_label = new QLabel(name, metric_card);
        name_label->setObjectName("compactMetricName");
        value_label = new QLabel(QStringLiteral("--"), metric_card);
        value_label->setObjectName("compactMetricValue");
        value_label->setWordWrap(true);

        metric_layout->addWidget(name_label);
        metric_layout->addWidget(value_label);
        metrics_layout->addWidget(metric_card);
    };

    create_metric(QStringLiteral("图像X"), image_x_label);
    create_metric(QStringLiteral("图像Y"), image_y_label);
    create_metric(QStringLiteral("机械X"), machine_x_label);
    create_metric(QStringLiteral("机械Y"), machine_y_label);
    create_metric(QStringLiteral("角度"), angle_label);
    create_metric(QStringLiteral("D%1").arg(registers.pick_status), pick_status_label);
    create_metric(QStringLiteral("D%1").arg(registers.head_type), head_type_label);

    card_layout->addLayout(metrics_layout);
    return card;
}

void GraspMainWindow::refreshTargetTable()
{
    if (!target_list_content_layout_ || !target_list_content_) {
        return;
    }

    if (!target_empty_label_) {
        target_empty_label_ = new QLabel(QStringLiteral("当前无检测结果"), target_list_content_);
        target_empty_label_->setObjectName("pathHintLabel");
        target_empty_label_->setWordWrap(true);
        target_list_content_layout_->addWidget(target_empty_label_);
    }

    QList<int> visible_indices;
    if (show_all_detections_) {
        for (int i = 0; i < static_cast<int>(current_result_.detections.size()); ++i) {
            visible_indices.append(i);
        }
    } else {
        const int primary_index = current_result_.primary_index;
        if (primary_index >= 0 && primary_index < static_cast<int>(current_result_.detections.size())) {
            visible_indices.append(primary_index);
        }
    }

    const int required_cards = visible_indices.size();
    while (target_cards_.size() < required_cards) {
        TargetCard target_card;
        target_card.card = createTargetCard(target_card.title_label,
                                            target_card.image_x_label,
                                            target_card.image_y_label,
                                            target_card.machine_x_label,
                                            target_card.machine_y_label,
                                            target_card.angle_label,
                                            target_card.pick_status_label,
                                            target_card.head_type_label);
        int insert_index = target_list_content_layout_->count();
        if (insert_index > 0 &&
            target_list_content_layout_->itemAt(insert_index - 1)->spacerItem() != nullptr) {
            --insert_index;
        }
        target_list_content_layout_->insertWidget(insert_index, target_card.card);
        target_cards_.append(target_card);
    }

    target_empty_label_->setVisible(required_cards == 0);
    for (int i = 0; i < target_cards_.size(); ++i) {
        TargetCard& target_card = target_cards_[i];
        const bool visible = i < required_cards;
        target_card.card->setVisible(visible);
        if (!visible) {
            continue;
        }

        const int detection_index = visible_indices[i];
        const auto& detection = current_result_.detections[detection_index];
        target_card.title_label->setText(show_all_detections_
                                             ? QStringLiteral("序号 %1").arg(detection_index + 1)
                                             : QStringLiteral("主目标"));
        SetValueText(target_card.image_x_label, FormatNumber(detection.center_x));
        SetValueText(target_card.image_y_label, FormatNumber(detection.center_y));
        SetValueText(target_card.machine_x_label, FormatMachineCoordinate(detection, true));
        SetValueText(target_card.machine_y_label, FormatMachineCoordinate(detection, false));
        SetValueText(target_card.angle_label, FormatNumber(calibratedAngle(detection.angle_deg)) + QStringLiteral(" deg"));
        SetValueText(target_card.pick_status_label, QString::number(detection.pick_status_code));
        SetValueText(target_card.head_type_label,
                     detection.head_type_code > 0
                         ? QStringLiteral("%1 %2")
                               .arg(detection.head_type_code)
                               .arg(QString::fromStdString(detection.head_type_text))
                         : QStringLiteral("--"));
    }

    if (target_list_content_layout_->count() == 0 ||
        target_list_content_layout_->itemAt(target_list_content_layout_->count() - 1)->spacerItem() == nullptr) {
        target_list_content_layout_->addStretch();
    }
}

void GraspMainWindow::showDetailPage()
{
    if (side_pages_ && detail_page_) {
        side_pages_->setCurrentWidget(detail_page_);
    }
}

void GraspMainWindow::showTargetListPage()
{
    if (side_pages_ && target_list_page_) {
        side_pages_->setCurrentWidget(target_list_page_);
    }
}

void GraspMainWindow::refreshRuntimePresentation()
{
    refreshDeviceStatus();
    refreshRuntimeStrip();
}

void GraspMainWindow::refreshResultPresentation(bool render_frame)
{
    if (render_frame) {
        renderCurrentFrame();
    }
    refreshInfoPanel();
    refreshTargetTable();
    refreshRuntimePresentation();
}

RuntimeStatusSnapshot GraspMainWindow::runtimeStatusSnapshot() const
{
    RuntimeStatusSnapshot snapshot;
    snapshot.input_mode = input_mode_ == InputMode::Camera
        ? RuntimeInputMode::Camera
        : (input_mode_ == InputMode::Image ? RuntimeInputMode::Image : RuntimeInputMode::Idle);
    snapshot.workflow_state = plc_runtime_state_.workflowState();
    snapshot.result = current_result_;
    snapshot.plc_runtime_status_text = plc_runtime_state_.statusText();
    snapshot.camera_running = workflow_.isCameraRunning();
    snapshot.models_loaded = workflow_.areModelsLoaded();
    snapshot.models_loading = models_loading_;
    snapshot.image_detection_running = image_detection_running_;
    snapshot.plc_link_active = plc_runtime_state_.active();
    snapshot.plc_test_running = plc_test_watcher_ && !plc_test_watcher_->isFinished();
    snapshot.show_all_detections = show_all_detections_;
    snapshot.has_input = !current_frame_.empty() || input_mode_ == InputMode::Camera;
    snapshot.has_current_frame = !current_frame_.empty();
    return snapshot;
}

void GraspMainWindow::refreshDeviceStatus()
{
    const RuntimeStatusSnapshot snapshot = runtimeStatusSnapshot();
    DeviceStatusView view;
    view.camera_dot = camera_status_dot_;
    view.camera_text = camera_status_value_;
    view.model_dot = obb_status_dot_;
    view.model_text = obb_status_value_;
    view.open_camera_button = open_camera_button_;
    view.start_button = start_button_;
    view.plc_link_button = plc_link_button_;
    view.plc_test_button = plc_test_button_;
    view.stop_button = stop_button_;
    view.display_mode_button = display_mode_button_;
    RuntimeStatusPresenter::RefreshDeviceStatus(view, snapshot);
}

void GraspMainWindow::refreshRuntimeStrip()
{
    const RuntimeStatusSnapshot snapshot = runtimeStatusSnapshot();
    RuntimeStripView view;
    view.mode = runtime_mode_label_;
    view.state = runtime_state_label_;
    view.obb_count = runtime_obb_count_label_;
    view.seg_count = runtime_seg_count_label_;
    view.stage_time = runtime_stage_time_label_;
    view.total_time = runtime_total_time_label_;
    RuntimeStatusPresenter::RefreshRuntimeStrip(view, snapshot);
}

void GraspMainWindow::updateStatusMessage(const QString& message, int timeout_ms)
{
    statusBar()->showMessage(message, timeout_ms);
}

void GraspMainWindow::clearResults()
{
    current_result_ = FrameInferenceResult{};
    camera_ui_update_pending_ = false;
    refreshInfoPanel();
    refreshTargetTable();
    refreshRuntimeStrip();
}

void GraspMainWindow::setEmptyPreviewMessage(const QString& message)
{
    current_frame_.release();
    camera_ui_update_pending_ = false;
    image_label_->setPixmap(QPixmap());
    image_label_->setText(message);
    refreshRuntimeStrip();
}

void GraspMainWindow::setModelLoadingState(bool loading)
{
    models_loading_ = loading;
    refreshDeviceStatus();
    refreshRuntimeStrip();
}

void GraspMainWindow::loadModelsFromPathsAsync(const QString& obb_model_path,
                                               const QString& seg_model_path,
                                               bool show_error_dialog,
                                               bool notify_success)
{
    if (models_loading_) {
        return;
    }

    const QString trimmed_obb_path = obb_model_path.trimmed();
    const QString trimmed_seg_path = seg_model_path.trimmed();
    if (trimmed_obb_path.isEmpty() || trimmed_seg_path.isEmpty()) {
        if (show_error_dialog) {
            QMessageBox::warning(this,
                                 QStringLiteral("缺少模型"),
                                 QStringLiteral("请同时选择完整的模型文件。"));
        }
        return;
    }

    workflow_.stopCamera();
    setModelLoadingState(true);
    updateStatusMessage(QStringLiteral("正在加载模型..."));

    auto* watcher = new QFutureWatcher<QString>(this);
    model_load_watcher_ = watcher;
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, trimmed_obb_path, trimmed_seg_path, show_error_dialog, notify_success]() {
        const QString error_message = watcher->result();
        const bool ok = error_message.isEmpty();
        if (model_load_watcher_ == watcher) {
            model_load_watcher_ = nullptr;
        }
        watcher->deleteLater();
        setModelLoadingState(false);

        if (ok) {
            obb_model_path_ = trimmed_obb_path;
            seg_model_path_ = trimmed_seg_path;
            if (notify_success) {
                updateStatusMessage(QStringLiteral("模型已加载。"), 5000);
            }
        } else {
            if (show_error_dialog) {
                QMessageBox::critical(this, QStringLiteral("模型加载失败"), error_message);
            }
            updateStatusMessage(QStringLiteral("模型加载失败，请检查模型路径和运行环境。"), 6000);
        }

        refreshDeviceStatus();
        refreshRuntimeStrip();
        if (close_after_model_load_) {
            close_after_model_load_ = false;
            close();
        }
    });

    watcher->setFuture(QtConcurrent::run([this, trimmed_obb_path, trimmed_seg_path]() -> QString {
        OBBConfig obb_config;
        SEGConfig seg_config;
        obb_config.conf_threshold = static_cast<float>(obb_conf_threshold_);
        obb_config.nms_threshold = static_cast<float>(obb_nms_threshold_);
        seg_config.conf_threshold = static_cast<float>(seg_conf_threshold_);
        seg_config.nms_threshold = static_cast<float>(seg_nms_threshold_);
        obb_config.enable_warmup = true;
        seg_config.enable_warmup = true;
        std::string error_message;
        const bool ok = workflow_.loadModels(trimmed_obb_path.toStdString(),
                                             obb_config,
                                             trimmed_seg_path.toStdString(),
                                             seg_config,
                                             &error_message);
        if (!ok) {
            std::cerr << "[OpenVINO] model load failed: " << error_message << std::endl;
            return QString::fromStdString(error_message);
        }
        return QString();
    }));
}

QString GraspMainWindow::currentImagePath() const
{
    const QString path = current_image_path_.trimmed();
    if (!path.isEmpty()) {
        const QFileInfo info(path);
        if (info.exists()) {
            return info.absoluteFilePath();
        }
    }
    return QStringLiteral("./samples/images");
}













