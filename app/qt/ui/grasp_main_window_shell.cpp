#include "grasp_main_window.h"

#include "engineering_settings_dialog_controller.h"
#include "engineering_settings_dialog_helpers.h"
#include "grasp_main_window_common.h"
#include "grasp_main_window_scale.h"

#include <QApplication>
#include <QtConcurrent/QtConcurrent>
#include <QComboBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QDateTime>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFrame>
#include <QBoxLayout>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QScreen>
#include <QStyleOptionComboBox>
#include <QTimer>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QShowEvent>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolButton>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <utility>
namespace {

constexpr int kSidebarMinWidth = 96;

class ProjectProfileSelector final : public QComboBox
{
public:
    explicit ProjectProfileSelector(QWidget* parent = nullptr)
        : QComboBox(parent)
    {
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QComboBox::paintEvent(event);

        QStyleOptionComboBox option;
        initStyleOption(&option);
        QRect arrow_rect = style()->subControlRect(
            QStyle::CC_ComboBox, &option, QStyle::SC_ComboBoxArrow, this);
        if (!arrow_rect.isValid() || arrow_rect.width() < 12) {
            arrow_rect = QRect(width() - 28, 0, 28, height());
        }
        const int chevron_width = qMax(12, arrow_rect.width() / 2);
        const int chevron_height = qMax(7, arrow_rect.height() / 4);
        const QPoint center = arrow_rect.center();
        const int left_x = center.x() - chevron_width / 2;
        const int right_x = center.x() + chevron_width / 2;
        const int top_y = center.y() - chevron_height / 2;
        const int bottom_y = center.y() + chevron_height / 2;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPen pen(isEnabled() ? QColor(QStringLiteral("#f4f8fc"))
                             : QColor(QStringLiteral("#9aaab7")),
                 2.0);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(pen);
        painter.drawLine(left_x, top_y, center.x(), bottom_y);
        painter.drawLine(center.x(), bottom_y, right_x, top_y);
    }
};

class DisplayModeSelector final : public QComboBox
{
public:
    explicit DisplayModeSelector(QWidget* parent = nullptr)
        : QComboBox(parent)
    {
        setEditable(true);
        setInsertPolicy(QComboBox::NoInsert);
        if (lineEdit() != nullptr) {
            lineEdit()->setReadOnly(true);
            lineEdit()->setAlignment(Qt::AlignCenter);
            lineEdit()->setFrame(false);
            lineEdit()->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        }
    }

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event != nullptr && event->button() == Qt::LeftButton) {
            showPopup();
            event->accept();
            return;
        }
        QComboBox::mousePressEvent(event);
    }
};

void ClearLayout(QLayout* layout)
{
    if (layout == nullptr) {
        return;
    }
    while (QLayoutItem* item = layout->takeAt(0)) {
        delete item;
    }
}

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

QString AppDisplayLabel(const AppDisplayConfig& config)
{
    const QString name = config.name.trimmed().isEmpty() ? QStringLiteral("TankEye-Iris") : config.name.trimmed();
    const QString version = config.version.trimmed();
    return version.isEmpty() ? name : QStringLiteral("%1 V%2").arg(name, version);
}

QString AppTitleLabel(const AppDisplayConfig& config)
{
    const QString title = config.title.trimmed();
    return title.isEmpty() ? QStringLiteral("截止阀抓取上料系统") : title;
}

QString DisplayOverlayModeText(DisplayOverlayMode mode)
{
    return GraspMainWindowDisplayOverlayModeText(mode);
}

QString DisplayOverlayModeFileSuffix(DisplayOverlayMode mode)
{
    return GraspMainWindowDisplayOverlayModeFileSuffix(mode);
}

bool IsAllDebugMode(DisplayOverlayMode mode)
{
    return GraspMainWindowIsAllDebugMode(mode);
}

double UiScale()
{
    return GraspUiScale();
}

int S(int value)
{
    return GraspS(value);
}

QSize SS(int width, int height)
{
    return GraspSS(width, height);
}

double TopBarScale()
{
    return GraspTopBarScale();
}

int TS(int value)
{
    return GraspTS(value);
}

QSize TSS(int width, int height)
{
    return GraspTSS(width, height);
}

int SC(int value)
{
    return GraspSC(value);
}

QMargins SCM(int left, int top, int right, int bottom)
{
    return GraspSCM(left, top, right, bottom);
}

QMargins SM(int left, int top, int right, int bottom)
{
    return GraspSM(left, top, right, bottom);
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

QLabel* CreateRuntimeBadge(const QString& object_name)
{
    auto* label = new QLabel(QStringLiteral("--"));
    label->setObjectName(object_name);
    label->setAlignment(Qt::AlignCenter);
    return label;
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
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color, 1.9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(QPointF(size.width() * 0.28, size.height() * 0.28),
                     QPointF(size.width() * 0.72, size.height() * 0.72));
    painter.drawLine(QPointF(size.width() * 0.72, size.height() * 0.28),
                     QPointF(size.width() * 0.28, size.height() * 0.72));
    return QIcon(pixmap);
}

using IconFactory = QIcon (*)(const QSize&, const QColor&);

QToolButton* CreateIconToolButton(QWidget* parent,
                                  const QSize& icon_size,
                                  const QSize& button_size,
                                  const QColor& icon_color,
                                  const QString& tooltip,
                                  const char* property_name,
                                  IconFactory icon_factory)
{
    auto* button = new QToolButton(parent);
    button->setCursor(Qt::PointingHandCursor);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setFixedSize(button_size);
    button->setIconSize(icon_size);
    button->setToolTip(tooltip);
    button->setProperty(property_name, true);
    if (icon_factory) {
        button->setIcon(icon_factory(icon_size, icon_color));
    }
    return button;
}

void RefreshIconToolButton(QToolButton* button,
                           const QSize& icon_size,
                           const QSize& button_size,
                           const QColor& icon_color,
                           IconFactory icon_factory)
{
    if (!button) {
        return;
    }
    button->setFixedSize(button_size);
    button->setIconSize(icon_size);
    if (icon_factory) {
        button->setIcon(icon_factory(icon_size, icon_color));
    }
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
    loadCameraSettings();
    loadStartupLaunchSettings();
    loadProjectProfileSettings();
    display_overlay_mode_ = DisplayOverlayMode::NormalHidden;
    setupUi();
    applyStyles();
    bindActions();
    refreshProjectProfileSelector();
    refreshResultPresentation(false);
    setEmptyPreviewMessage(QStringLiteral("请选择图片或打开相机"));
    updateStatusMessage(QStringLiteral("界面已就绪，可在工程设置中加载模型。"));
}

GraspMainWindow::~GraspMainWindow()
{
    workflow_.stopCamera();
    waitForBackgroundJobs();
}

void GraspMainWindow::setDisplayOverlayMode(DisplayOverlayMode mode)
{
    display_overlay_mode_ = mode;
    refreshDeviceStatus();
    renderCurrentFrame();
    refreshTargetTable();
}

void GraspMainWindow::toggleImageSaveSession()
{
    image_save_active_ = !image_save_active_;
    if (image_save_active_) {
        image_save_overlay_mode_ = display_overlay_mode_;
        image_save_started_at_ = std::chrono::steady_clock::now();
        image_save_skip_next_frame_ = true;
        updateStatusMessage(QStringLiteral("PLC图片保存已开始：%1模式。")
                                .arg(DisplayOverlayModeText(image_save_overlay_mode_)),
                            5000);
    } else {
        updateStatusMessage(QStringLiteral("PLC图片保存已停止。"), 5000);
    }
    refreshImageSaveButton();
}

void GraspMainWindow::refreshImageSaveButton()
{
    if (!image_save_button_) {
        return;
    }
    image_save_button_->setText(image_save_active_ ? QStringLiteral("停止保存")
                                                   : QStringLiteral("开始保存"));
    image_save_button_->setToolTip(
        image_save_active_
            ? QStringLiteral("正在保存PLC检测帧：%1模式").arg(DisplayOverlayModeText(image_save_overlay_mode_))
            : QStringLiteral("开始保存PLC检测帧，使用当前显示模式"));
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
        } else {
            maybeStartAutoGrasp();
        }
    }
}

void GraspMainWindow::setupUi()
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    resize(SS(1360, 820));
    setMinimumSize(SS(960, 600));
    setWindowTitle(QStringLiteral("%1 - %2").arg(AppDisplayLabel(app_config_.app), AppTitleLabel(app_config_.app)));
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
    left_widget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
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

    brand_label_ = new QLabel(AppDisplayLabel(app_config_.app), top_bar_);
    brand_label_->setObjectName("brandLabel");
    brand_label_->setMinimumWidth(S(170));
    brand_label_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    brand_label_->setToolTip(AppDisplayLabel(app_config_.app));

    left_layout->addWidget(logo_label_);
    left_layout->addWidget(brand_label_);

    title_label_ = new QLabel(AppTitleLabel(app_config_.app), top_bar_);
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
    user_button_ = CreateIconToolButton(top_bar_, icon_size, button_size, icon_color, QStringLiteral("用户"), "topBarButton", CreateAvatarIcon);
    settings_icon_button_ = CreateIconToolButton(top_bar_, icon_size, button_size, icon_color, QStringLiteral("工程设置"), "topBarButton", CreateGearIcon);
    minimize_window_button_ = CreateIconToolButton(top_bar_, icon_size, button_size, icon_color, QStringLiteral("最小化"), "windowButton", CreateMinimizeIcon);
    maximize_window_button_ = CreateIconToolButton(top_bar_, icon_size, button_size, icon_color, QStringLiteral("最大化"), "windowButton", nullptr);
    close_window_button_ = CreateIconToolButton(top_bar_, icon_size, button_size, icon_color, QStringLiteral("关闭"), "closeWindowButton", CreateCloseIcon);

    right_layout->addWidget(user_button_);
    right_layout->addWidget(settings_icon_button_);
    right_layout->addSpacing(S(2));
    right_layout->addWidget(minimize_window_button_);
    right_layout->addWidget(maximize_window_button_);
    right_layout->addWidget(close_window_button_);

    top_layout->addWidget(title_label_, 0, 1, Qt::AlignCenter);
    top_layout->addWidget(left_widget, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
    top_layout->addWidget(top_right_controls_, 0, 2, Qt::AlignRight | Qt::AlignVCenter);
    top_layout->setColumnStretch(0, 0);
    top_layout->setColumnStretch(1, 1);
    top_layout->setColumnStretch(2, 0);
    top_layout->setColumnMinimumWidth(0, left_widget->sizeHint().width());
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
    result_section_title_ = CreateSectionTitle(QStringLiteral("当前目标参数"));
    result_title_row_ = new QHBoxLayout();
    result_title_row_->setContentsMargins(0, 0, 0, 0);
    result_title_row_->setSpacing(SC(6));
    result_title_row_->addWidget(result_section_title_, 1);
    project_profile_label_ = new QLabel(QStringLiteral("方案选择："), this);
    project_profile_label_->setObjectName("operatorProjectProfileLabel");
    project_profile_label_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    project_profile_selector_ = new ProjectProfileSelector(this);
    project_profile_selector_->setObjectName("operatorProjectProfileSelector");
    project_profile_selector_->setMinimumWidth(SC(96));
    project_profile_selector_->setMinimumHeight(SC(30));
    project_profile_selector_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    project_profile_selector_->setToolTip(QStringLiteral("选择物料方案；参数只能由管理员在工程设置中维护。"));
    side_layout->addLayout(result_title_row_);

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

    project_profile_card_ = new QFrame(this);
    project_profile_card_->setObjectName("controlGroupCard");
    project_profile_card_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    project_profile_card_layout_ = new QVBoxLayout(project_profile_card_);
    project_profile_card_layout_->setContentsMargins(SCM(6, 6, 6, 6));
    project_profile_card_layout_->setSpacing(SC(6));
    project_profile_section_title_ = CreateSectionTitle(QStringLiteral("方案切换"));
    project_profile_card_layout_->addWidget(project_profile_section_title_);
    side_layout->addWidget(project_profile_card_);
    refreshProjectProfilePlacement();
}

void GraspMainWindow::buildStatusSection(QVBoxLayout* side_layout)
{
    side_layout->addWidget(CreateSectionTitle(QStringLiteral("系统状态")));

    status_card_ = new QFrame(this);
    status_card_->setObjectName("infoCard");
    status_card_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    status_layout_ = new QBoxLayout(QBoxLayout::TopToBottom, status_card_);
    status_layout_->setContentsMargins(SCM(6, 6, 6, 6));
    status_layout_->setSpacing(SC(4));

    camera_status_dot_ = new QLabel(this);
    camera_status_value_ = new QLabel(this);
    obb_status_dot_ = new QLabel(this);
    obb_status_value_ = new QLabel(this);
    seg_status_dot_ = new QLabel(this);
    seg_status_value_ = new QLabel(this);
    status_divider_ = new QFrame(this);
    status_divider_->setFrameShape(QFrame::VLine);
    status_divider_->setFrameShadow(QFrame::Plain);
    status_divider_->setObjectName("statusDivider");
    status_divider_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    const auto add_status_item = [this](const QString& name, QLabel* dot, QLabel* value) {
        auto* container = new QFrame(this);
        container->setFrameShape(QFrame::NoFrame);
        container->setFrameShadow(QFrame::Plain);
        container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        auto* layout = new QHBoxLayout(container);
        layout->setContentsMargins(SCM(5, 3, 5, 3));
        layout->setSpacing(SC(4));
        auto* label = new QLabel(name, container);
        label->setObjectName("statusStackName");
        label->setMinimumWidth(0);
        label->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
        status_name_labels_.push_back(label);
        layout->addWidget(label);
        layout->addWidget(dot);
        status_dot_labels_.push_back(dot);
        value->setObjectName("statusStackValue");
        status_value_labels_.push_back(value);
        value->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
        status_item_frames_.push_back(container);
        status_layout_->addWidget(container);
    };

    add_status_item(QStringLiteral("相机"), camera_status_dot_, camera_status_value_);
    add_status_item(QStringLiteral("模型"), obb_status_dot_, obb_status_value_);
    refreshStatusSectionMode();
    side_layout->addWidget(status_card_);
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
    plc_link_button_ = new QPushButton(QStringLiteral("开始"), this);
    plc_test_button_ = new QPushButton(QStringLiteral("测试"), this);
    stop_button_ = new QPushButton(QStringLiteral("关闭相机"), this);
    display_mode_selector_ = new DisplayModeSelector(this);
    display_mode_selector_->addItem(QStringLiteral("隐藏"),
                                    static_cast<int>(DisplayOverlayMode::NormalHidden));
    display_mode_selector_->addItem(QStringLiteral("全显"),
                                    static_cast<int>(DisplayOverlayMode::AllDebug));
    display_mode_selector_->addItem(QStringLiteral("无显示"),
                                    static_cast<int>(DisplayOverlayMode::None));
    display_mode_selector_->setCurrentIndex(0);
    display_mode_selector_->setObjectName(QStringLiteral("displayModeSelector"));
    image_save_button_ = new QPushButton(QStringLiteral("开始保存"), this);
    target_list_button_ = new QPushButton(QStringLiteral("目标列表"), this);
    engineering_button_ = new QPushButton(QStringLiteral("工程设置"), this);
    runtime_log_button_ = new QPushButton(QStringLiteral("运行日志"), this);

    const QList<QPushButton*> action_buttons = {
        load_image_button_, open_camera_button_, start_button_, plc_link_button_,
        plc_test_button_, stop_button_, image_save_button_,
        target_list_button_, engineering_button_, runtime_log_button_
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
    display_mode_selector_->setMinimumHeight(secondary_button_height);
    display_mode_selector_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    image_save_button_->setMinimumHeight(secondary_button_height);
    target_list_button_->setMinimumHeight(secondary_button_height);
    engineering_button_->setMinimumHeight(secondary_button_height);
    runtime_log_button_->setMinimumHeight(secondary_button_height);
    image_save_button_->setObjectName("secondaryActionButton");
    target_list_button_->setObjectName("secondaryActionButton");
    engineering_button_->setObjectName("secondaryActionButton");
    runtime_log_button_->setObjectName("secondaryActionButton");
    plc_link_button_->setObjectName("primaryActionButton");
    stop_button_->setObjectName("stopActionButton");

    input_group_ = new QFrame(this);
    input_group_->setObjectName("controlGroupCard");
    input_group_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* input_layout = new QVBoxLayout(input_group_);
    input_layout->setContentsMargins(SCM(6, 6, 6, 6));
    input_layout->setSpacing(SC(4));
    input_layout->addWidget(CreateGroupCaption(QStringLiteral("输入来源"), input_group_));
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

    detect_buttons_layout_ = new QGridLayout();
    detect_buttons_layout_->setContentsMargins(0, 0, 0, 0);
    detect_buttons_layout_->setHorizontalSpacing(SC(6));
    detect_buttons_layout_->setVerticalSpacing(SC(6));
    detect_buttons_layout_->addWidget(start_button_, 0, 0);
    detect_buttons_layout_->addWidget(plc_test_button_, 0, 1);
    detect_buttons_layout_->addWidget(plc_link_button_, 1, 0);
    detect_buttons_layout_->addWidget(stop_button_, 1, 1);
    detect_buttons_layout_->setColumnStretch(0, 1);
    detect_buttons_layout_->setColumnStretch(1, 1);
    detect_layout->addLayout(detect_buttons_layout_);

    side_layout->addWidget(input_group_);
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

void GraspMainWindow::refreshProjectProfilePlacement()
{
    if (!result_title_row_ || !project_profile_card_ || !project_profile_card_layout_ ||
        !project_profile_selector_ || !project_profile_label_ || !result_section_title_ ||
        !project_profile_section_title_) {
        return;
    }

    ClearLayout(result_title_row_);
    ClearLayout(project_profile_card_layout_);
    result_title_row_->addWidget(result_section_title_, 1);

    if (admin_mode_) {
        project_profile_card_->setVisible(false);
        project_profile_label_->setVisible(true);
        result_title_row_->addWidget(project_profile_label_, 0, Qt::AlignVCenter);
        result_title_row_->addWidget(project_profile_selector_, 0, Qt::AlignVCenter);
    } else {
        project_profile_label_->setVisible(false);
        project_profile_card_->setVisible(true);
        project_profile_card_layout_->addWidget(project_profile_section_title_);
        project_profile_card_layout_->addWidget(project_profile_selector_);
    }
}

void GraspMainWindow::refreshStatusSectionMode()
{
    if (!status_card_ || !status_layout_) {
        return;
    }

    while (QLayoutItem* item = status_layout_->takeAt(0)) {
        delete item;
    }

    const bool compact_mode = admin_mode_;
    status_layout_->setDirection(compact_mode ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    status_layout_->setSpacing(compact_mode ? SC(10) : SC(0));
    status_layout_->setContentsMargins(compact_mode ? SCM(6, 6, 6, 6) : SCM(12, 12, 12, 12));
    status_card_->setMaximumHeight(compact_mode ? QWIDGETSIZE_MAX : SC(168));
    status_card_->setMinimumHeight(compact_mode ? 0 : SC(168));
    if (status_divider_ != nullptr) {
        status_divider_->setVisible(!compact_mode);
        status_divider_->setFixedSize(SC(1), SC(76));
        status_divider_->setStyleSheet(QStringLiteral("background: #4b6478;"));
    }

    for (int i = 0; i < status_item_frames_.size(); ++i) {
        QFrame* container = status_item_frames_.at(i);
        if (container != nullptr) {
            container->setObjectName(compact_mode ? QStringLiteral("statusStackItem") : QString());
            container->setSizePolicy(compact_mode ? QSizePolicy::Minimum : QSizePolicy::Expanding,
                                     QSizePolicy::Preferred);
            container->setMinimumHeight(compact_mode ? SC(28) : SC(72));
            container->setMaximumHeight(compact_mode ? QWIDGETSIZE_MAX : SC(72));
            if (container->style() != nullptr) {
                container->style()->unpolish(container);
                container->style()->polish(container);
            }
            if (QBoxLayout* item_layout = qobject_cast<QBoxLayout*>(container->layout())) {
                ClearLayout(item_layout);
                item_layout->setContentsMargins(compact_mode ? SCM(5, 3, 5, 3) : SCM(0, 0, 0, 0));
                item_layout->setSpacing(compact_mode ? SC(4) : SC(6));
                item_layout->setAlignment(compact_mode ? Qt::AlignLeft : Qt::AlignCenter);

                QLabel* name_label = i < status_name_labels_.size() ? status_name_labels_.at(i) : nullptr;
                QLabel* dot_label = i < status_dot_labels_.size() ? status_dot_labels_.at(i) : nullptr;
                QLabel* value_label = i < status_value_labels_.size() ? status_value_labels_.at(i) : nullptr;
                if (name_label != nullptr) {
                    item_layout->addWidget(name_label);
                }
                if (dot_label != nullptr) {
                    item_layout->addWidget(dot_label);
                }
                if (compact_mode && value_label != nullptr) {
                    item_layout->addWidget(value_label, 1);
                }
            }
        }
    }
    if (compact_mode) {
        for (QFrame* container : status_item_frames_) {
            if (container != nullptr) {
                status_layout_->addWidget(container);
            }
        }
    } else if (status_item_frames_.size() >= 2) {
        status_layout_->addWidget(status_item_frames_.at(0), 1);
        if (status_divider_ != nullptr) {
            status_layout_->addWidget(status_divider_, 0, Qt::AlignVCenter);
        }
        status_layout_->addWidget(status_item_frames_.at(1), 1);
    }

    const int dot_size = compact_mode ? SC(16) : S(34);
    const int dot_radius = dot_size / 2;
    const int name_font_size = compact_mode ? SC(15) : S(34);
    const int value_font_size = compact_mode ? SC(15) : SC(20);
    for (QLabel* label : status_dot_labels_) {
        if (label != nullptr) {
            label->setFixedSize(dot_size, dot_size);
            label->setStyleSheet(QStringLiteral("border-radius: %1px; background:%2;")
                                     .arg(dot_radius)
                                     .arg(label == camera_status_dot_ || label == obb_status_dot_
                                              ? QStringLiteral("#86d779")
                                              : QStringLiteral("#d46a6a")));
        }
    }
    for (QLabel* label : status_name_labels_) {
        if (label != nullptr) {
            label->setStyleSheet(QStringLiteral("color: #dbe7f1; font-size: %1px; font-weight: 600;")
                                     .arg(name_font_size));
        }
    }
    for (QLabel* label : status_value_labels_) {
        if (label != nullptr) {
            label->setVisible(compact_mode);
            label->setStyleSheet(QStringLiteral("color: #f4f8fc; font-size: %1px; font-weight: 700;")
                                     .arg(value_font_size));
        }
    }
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
        "#brandLabel { color: #ffffff; font-size: %7px; font-weight: 700; }"
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
        "#statusStackValue { color: #f4f8fc; }"
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
        "QComboBox#displayModeSelector { background: rgba(255,255,255,0.06); color: #ecf4fa; border: 1px solid #36546b; border-radius: %60px; padding: %61px %62px; font-size: %59px; font-weight: 600; }"
        "QComboBox#displayModeSelector:hover { background: rgba(255,255,255,0.12); }"
        "QComboBox#displayModeSelector::drop-down { width: 0px; border: none; }"
        "QComboBox#displayModeSelector::down-arrow { width: 0px; height: 0px; image: none; }"
        "QComboBox#displayModeSelector QLineEdit { background: transparent; border: none; padding: 0px; color: #ecf4fa; selection-background-color: transparent; selection-color: #ecf4fa; }"
        "QComboBox#displayModeSelector QAbstractItemView { background: #182735; color: #f4f8fc; border: 1px solid #4e718a; selection-background-color: #236c9f; selection-color: #ffffff; }"
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
        .arg(SC(5)).arg(SC(10)).arg(SC(12)).arg(SC(12))
        .arg(S(8)).arg(S(4)).arg(S(8)).arg(S(26));
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
    g_grasp_responsive_ui_scale = next_scale;
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
        brand_label_->setMinimumWidth(TS(170));
        brand_label_->setMaximumWidth(TS(220));
        brand_label_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
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
        RefreshIconToolButton(button, icon_size, button_size, icon_color, nullptr);
    }
    if (top_right_controls_) {
        if (auto* right_layout = qobject_cast<QHBoxLayout*>(top_right_controls_->layout())) {
            right_layout->setSpacing(TS(6));
            right_layout->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        }
        int visible_button_count = 0;
        for (QToolButton* button : buttons) {
            if (button && !button->isHidden()) {
                ++visible_button_count;
            }
        }
        visible_button_count = qMax(1, visible_button_count);
        const int controls_width =
            button_size.width() * visible_button_count +
            TS(6) * qMax(0, visible_button_count - 1) +
            TS(2);
        top_right_controls_->setMinimumWidth(controls_width);
        top_right_controls_->setMaximumWidth(controls_width);
        if (auto* top_layout = qobject_cast<QGridLayout*>(top_bar_->layout())) {
            if (logo_label_ && brand_label_) {
                top_layout->setColumnMinimumWidth(0, logo_label_->width() + brand_label_->minimumWidth() + TS(10));
            }
            top_layout->setColumnMinimumWidth(2, controls_width);
            top_layout->setColumnStretch(0, 0);
            top_layout->setColumnStretch(1, 1);
            top_layout->setColumnStretch(2, 0);
            top_layout->setAlignment(title_label_, Qt::AlignCenter);
            top_layout->setAlignment(top_right_controls_, Qt::AlignRight | Qt::AlignVCenter);
        }
    }
    RefreshIconToolButton(user_button_, icon_size, button_size, icon_color, CreateAvatarIcon);
    RefreshIconToolButton(settings_icon_button_, icon_size, button_size, icon_color, CreateGearIcon);
    RefreshIconToolButton(minimize_window_button_, icon_size, button_size, icon_color, CreateMinimizeIcon);
    RefreshIconToolButton(close_window_button_, icon_size, button_size, icon_color, CreateCloseIcon);
}

void GraspMainWindow::refreshActionButtonMetrics()
{
    const QList<QPushButton*> primary_buttons = {
        load_image_button_, open_camera_button_, start_button_, plc_link_button_, plc_test_button_, stop_button_
    };
    const QList<QPushButton*> secondary_buttons = {
        image_save_button_, target_list_button_, engineering_button_,
        runtime_log_button_, target_list_back_button_
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
    if (display_mode_selector_) {
        display_mode_selector_->setMinimumHeight(SC(34));
        display_mode_selector_->setMinimumWidth(SC(96));
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
    g_grasp_sidebar_compact_scale = qBound(0.48,
                                           width_basis / static_cast<double>(S(kSidebarCompactBaseWidth)),
                                           1.0);
    if (project_profile_selector_) {
        const int selector_min_width = admin_mode_ ? SC(96) : SC(120);
        const int selector_min_height = admin_mode_ ? SC(30) : SC(38);
        const int selector_font_size = admin_mode_ ? SC(14) : SC(16);
        project_profile_selector_->setMinimumWidth(selector_min_width);
        project_profile_selector_->setMinimumHeight(selector_min_height);
        project_profile_selector_->setStyleSheet(QString(
            "QComboBox {"
            " background: rgba(8,16,25,0.72);"
            " color: #f4f8fc;"
            " border: 1px solid #4e718a;"
            " border-radius: %1px;"
            " padding: 0px %2px;"
            " font-size: %3px;"
            " min-height: %4px;"
            "}"
            "QComboBox:hover { border-color: #78a9c7; background: rgba(20,42,58,0.88); }"
            "QComboBox:focus { border-color: #3ca7ef; }"
            "QComboBox:disabled { color: #9aaab7; border-color: #465968; background: rgba(40,53,63,0.72); }"
            "QComboBox::drop-down {"
            " width: %5px;"
            " border: none;"
            " border-left: 1px solid #4e718a;"
            " border-top-right-radius: %1px;"
            " border-bottom-right-radius: %1px;"
            "}"
            "QComboBox::down-arrow { width: %6px; height: %6px; }"
            "QAbstractItemView {"
            " background: #182735;"
            " color: #f4f8fc;"
            " border: 1px solid #4e718a;"
            " border-radius: %1px;"
            " padding: %7px;"
            " selection-background-color: #236c9f;"
            " selection-color: #ffffff;"
            "}")
            .arg(SC(8))
            .arg(SC(8))
            .arg(selector_font_size)
            .arg(selector_min_height)
            .arg(admin_mode_ ? SC(28) : SC(30))
            .arg(SC(12))
            .arg(SC(4)));
    }
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

void GraspMainWindow::bindActions()
{
    connect(load_image_button_, &QPushButton::clicked, this, [this]() { loadImage(); });
    connect(open_camera_button_, &QPushButton::clicked, this, [this]() { openCamera(); });
    connect(start_button_, &QPushButton::clicked, this, [this]() { startDetection(); });
    connect(plc_link_button_, &QPushButton::clicked, this, [this]() { togglePlcLinkMode(); });
    connect(plc_test_button_, &QPushButton::clicked, this, [this]() { writePlcTestValues(); });
    connect(stop_button_, &QPushButton::clicked, this, [this]() { closeCamera(); });
    connect(display_mode_selector_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        const QVariant data = display_mode_selector_->itemData(index);
        setDisplayOverlayMode(static_cast<DisplayOverlayMode>(data.toInt()));
    });
    connect(image_save_button_, &QPushButton::clicked, this, [this]() { toggleImageSaveSession(); });
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
    connect(project_profile_selector_,
            QOverload<const QString&>::of(&QComboBox::activated),
            this,
            [this](const QString& profile_name) {
                handleOperatorProjectProfileSelection(profile_name);
            });
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

void GraspMainWindow::stopRuntimeForProjectProfileSwitch()
{
    workflow_.stopCamera();
    stopPlcLinkState(RuntimeWorkflowState::Idle);
    input_mode_ = InputMode::Idle;
    clearResults();
    refreshDeviceStatus();
    refreshRuntimeStrip();
}

bool GraspMainWindow::switchProjectProfile(const QString& profile_name,
                                           bool reload_models,
                                           QString* error_message)
{
    if (models_loading_) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("模型正在加载，请稍后再切换工程方案。");
        }
        return false;
    }

    ProjectProfileSettings profile;
    if (!LoadProjectProfile(profile_name, &profile, error_message)) {
        return false;
    }
    if (!validateProjectProfileModels(profile, error_message)) {
        return false;
    }

    has_pending_engineering_profile_settings_ = false;
    pending_engineering_profile_name_.clear();
    has_pending_activation_previous_profile_settings_ = false;
    pending_start_action_ = PendingStartAction::None;
    stopRuntimeForProjectProfileSwitch();
    ProjectProfileSettings previous_profile;
    QString previous_error;
    if (LoadProjectProfile(active_project_profile_name_, &previous_profile, &previous_error)) {
        pending_previous_project_profile_settings_ = previous_profile;
        has_pending_previous_project_profile_settings_ = true;
    }
    applyProjectProfileSettings(profile);
    refreshResultPresentation(true);
    updateStatusMessage(QStringLiteral("已切换工程方案：%1").arg(current_project_profile_name_), 5000);

    if (reload_models) {
        pending_project_profile_name_ = current_project_profile_name_;
        loadModelsFromPathsAsync(obb_model_path_, seg_model_path_, true, true);
    } else {
        active_project_profile_name_ = current_project_profile_name_;
        SaveActiveProjectProfileName(active_project_profile_name_);
        has_pending_previous_project_profile_settings_ = false;
    }
    return true;
}

bool GraspMainWindow::SaveEngineeringProfile(const EngineeringSettingsDraft& draft,
                                             const QString& profile_name,
                                             ProjectProfileSettings* saved_profile,
                                             QString* error_message)
{
    const QString trimmed_profile_name = profile_name.trimmed();
    if (trimmed_profile_name.isEmpty()) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("工程方案名称不能为空。");
        }
        return false;
    }
    const ProjectProfileSettings profile_to_save = projectProfileSettingsFromDraft(draft,
                                                                                   profile_name);
    if (trimmed_profile_name == active_project_profile_name_.trimmed() &&
        !validateProjectProfileModels(profile_to_save, error_message)) {
        return false;
    }
    return persistEngineeringProfile(profile_to_save, saved_profile, error_message);
}

bool GraspMainWindow::canSaveEngineeringProfile(QString* error_message) const
{
    if (models_loading_) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("模型加载完成后再保存工程方案。");
        }
        return false;
    }
    return true;
}

bool GraspMainWindow::StageActiveEngineeringProfile(const ProjectProfileSettings& saved_profile,
                                                    QString* error_message)
{
    Q_UNUSED(error_message);
    const QString trimmed_profile_name = saved_profile.name.trimmed();
    if (trimmed_profile_name.isEmpty() ||
        trimmed_profile_name != active_project_profile_name_.trimmed()) {
        return true;
    }

    pending_engineering_profile_settings_ = saved_profile;
    pending_engineering_profile_name_ = trimmed_profile_name;
    has_pending_engineering_profile_settings_ = true;
    updateStatusMessage(QStringLiteral("%1 已保存，下一次开始时生效；当前运行仍使用旧参数。")
                            .arg(trimmed_profile_name),
                        6000);
    return true;
}

bool GraspMainWindow::ApplyEngineeringProfile(const EngineeringSettingsDraft& draft,
                                              const QString& profile_name,
                                              QString* error_message)
{
    const ProjectProfileSettings profile_to_save = projectProfileSettingsFromDraft(draft,
                                                                                   profile_name);
    ProjectProfileSettings saved_profile;
    if (!persistEngineeringProfile(profile_to_save, &saved_profile, error_message)) {
        return false;
    }
    return switchProjectProfile(profile_name, true, error_message);
}

bool GraspMainWindow::CreateEngineeringProfile(const QString& profile_name,
                                               QString* saved_name,
                                               QString* error_message)
{
    const ProjectProfileSettings blank_profile = CreateBlankProjectProfile(profile_name);
    ProjectProfileSettings saved_profile;
    if (!persistEngineeringProfile(blank_profile, &saved_profile, error_message)) {
        return false;
    }
    if (saved_name != nullptr) {
        *saved_name = saved_profile.name;
    }
    return true;
}

bool GraspMainWindow::persistEngineeringProfile(const ProjectProfileSettings& profile,
                                                 ProjectProfileSettings* saved_profile,
                                                 QString* error_message)
{
    QString saved_name;
    if (!SaveProjectProfile(profile, &saved_name, error_message)) {
        return false;
    }

    if (saved_profile != nullptr) {
        *saved_profile = profile;
        saved_profile->name = saved_name;
    }
    return true;
}

bool GraspMainWindow::validateProjectProfileModels(const ProjectProfileSettings& profile,
                                                   QString* error_message) const
{
    const QStringList model_paths = { profile.obb_model_path, profile.seg_model_path };
    for (const QString& model_path : model_paths) {
        const QFileInfo xml_info(model_path);
        if (model_path.trimmed().isEmpty() || !xml_info.exists()) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("方案“%1”的模型文件不存在或未配置：%2")
                                     .arg(profile.name, model_path.isEmpty() ? QStringLiteral("空路径") : model_path);
            }
            return false;
        }
        const QString bin_path = QDir(xml_info.absolutePath()).filePath(xml_info.completeBaseName() + QStringLiteral(".bin"));
        if (!QFileInfo::exists(bin_path)) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("方案“%1”的模型 BIN 文件不存在：%2").arg(profile.name, bin_path);
            }
            return false;
        }
    }
    return true;
}

GraspMainWindow::PendingActivationResult
GraspMainWindow::activatePendingEngineeringSettingsForStart(PendingStartAction action,
                                                            QString* error_message)
{
    if (!has_pending_engineering_profile_settings_) {
        return PendingActivationResult::Ready;
    }
    if (models_loading_) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("模型正在加载，请稍候。");
        }
        return PendingActivationResult::Failed;
    }

    const ProjectProfileSettings next_profile = pending_engineering_profile_settings_;
    if (!validateProjectProfileModels(next_profile, error_message)) {
        return PendingActivationResult::Failed;
    }

    const ProjectProfileSettings previous_profile =
        currentProjectProfileSettings(active_project_profile_name_);
    const bool model_configuration_changed =
        previous_profile.obb_model_path != next_profile.obb_model_path ||
        previous_profile.seg_model_path != next_profile.seg_model_path ||
        previous_profile.model_thresholds.obb_conf_threshold != next_profile.model_thresholds.obb_conf_threshold ||
        previous_profile.model_thresholds.obb_nms_threshold != next_profile.model_thresholds.obb_nms_threshold ||
        previous_profile.model_thresholds.seg_conf_threshold != next_profile.model_thresholds.seg_conf_threshold ||
        previous_profile.model_thresholds.seg_nms_threshold != next_profile.model_thresholds.seg_nms_threshold ||
        !workflow_.areModelsLoaded();

    pending_activation_previous_profile_settings_ = previous_profile;
    has_pending_activation_previous_profile_settings_ = true;
    pending_start_action_ = action;
    applyProjectProfileSettings(next_profile);

    if (model_configuration_changed) {
        loadModelsFromPathsAsync(
            next_profile.obb_model_path,
            next_profile.seg_model_path,
            true,
            true,
            [this, action](bool success, const QString& load_error) {
                finishPendingEngineeringSettingsActivation(success, load_error, action);
            });
        return PendingActivationResult::Loading;
    }

    has_pending_engineering_profile_settings_ = false;
    pending_engineering_profile_name_.clear();
    has_pending_activation_previous_profile_settings_ = false;
    pending_start_action_ = PendingStartAction::None;
    SaveActiveProjectProfileName(active_project_profile_name_);
    updateStatusMessage(QStringLiteral("已应用保存的工程方案：%1。")
                            .arg(active_project_profile_name_),
                        5000);
    return PendingActivationResult::Ready;
}

void GraspMainWindow::finishPendingEngineeringSettingsActivation(
    bool success,
    const QString& error_message,
    PendingStartAction action)
{
    if (!success) {
        if (has_pending_activation_previous_profile_settings_) {
            applyProjectProfileSettings(pending_activation_previous_profile_settings_);
        }
        has_pending_activation_previous_profile_settings_ = false;
        pending_start_action_ = PendingStartAction::None;
        updateStatusMessage(
            error_message.trimmed().isEmpty()
                ? QStringLiteral("待生效工程方案应用失败，当前仍使用旧参数。")
                : QStringLiteral("待生效工程方案应用失败，当前仍使用旧参数：%1").arg(error_message),
            6000);
        return;
    }

    has_pending_engineering_profile_settings_ = false;
    pending_engineering_profile_name_.clear();
    has_pending_activation_previous_profile_settings_ = false;
    pending_start_action_ = PendingStartAction::None;
    SaveActiveProjectProfileName(active_project_profile_name_);
    updateStatusMessage(QStringLiteral("已应用保存的工程方案：%1。")
                            .arg(active_project_profile_name_),
                        5000);

    if (action == PendingStartAction::ImageDetection) {
        startDetection();
    } else if (action == PendingStartAction::PlcGrasp) {
        togglePlcLinkMode();
    }
}

void GraspMainWindow::refreshProjectProfileSelector()
{
    if (project_profile_selector_ == nullptr) {
        return;
    }

    QSignalBlocker blocker(project_profile_selector_);
    const QString active_name = active_project_profile_name_.trimmed();
    project_profile_selector_->clear();
    project_profile_selector_->addItems(ListProjectProfileNames());
    const int active_index = project_profile_selector_->findText(active_name);
    if (active_index >= 0) {
        project_profile_selector_->setCurrentIndex(active_index);
    }
    project_profile_selector_->setEnabled(!models_loading_);
    project_profile_selector_->setToolTip(
        models_loading_
            ? QStringLiteral("方案正在切换，请等待模型加载完成。")
            : QStringLiteral("选择物料方案；参数只能由管理员在工程设置中维护。"));
}

void GraspMainWindow::handleOperatorProjectProfileSelection(const QString& profile_name)
{
    const QString target_name = profile_name.trimmed();
    if (target_name.isEmpty() || target_name == active_project_profile_name_) {
        refreshProjectProfileSelector();
        return;
    }
    if (models_loading_) {
        refreshProjectProfileSelector();
        return;
    }

    const auto choice = QMessageBox::question(
        this,
        QStringLiteral("切换物料方案"),
        QStringLiteral("将停止当前检测、相机和 PLC 流程，清空当前结果并加载方案“%1”。是否继续？")
            .arg(target_name),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (choice != QMessageBox::Yes) {
        refreshProjectProfileSelector();
        return;
    }

    QString error_message;
    if (!switchProjectProfile(target_name, true, &error_message)) {
        QMessageBox::warning(this, QStringLiteral("切换方案失败"), error_message);
        refreshProjectProfileSelector();
        return;
    }

    refreshProjectProfileSelector();
    updateStatusMessage(QStringLiteral("正在切换物料方案：%1，请等待模型加载完成。").arg(target_name), 6000);
}

void GraspMainWindow::startPlcPollingState()
{
    plc_runtime_state_.startPolling();
    if (plc_poll_timer_) {
        plc_poll_timer_->start();
    }
}
