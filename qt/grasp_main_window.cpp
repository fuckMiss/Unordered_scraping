#include "grasp_main_window.h"

#include "frame_overlay.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDialog>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QShowEvent>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <iostream>

using namespace cv;
using namespace std;

namespace {

constexpr int kSidebarMinWidth = 340;
constexpr int kSidebarDefaultWidth = 360;
constexpr int kSidebarMaxWidth = 420;
constexpr int kSidebarToggleWidth = 24;
constexpr int kSidebarToggleHeight = 84;

void SetIndicator(QLabel* dot, QLabel* text, bool active, const QString& active_text, const QString& inactive_text)
{
    dot->setFixedSize(16, 16);
    dot->setStyleSheet(QString("border-radius: 8px; background:%1;").arg(active ? "#86d779" : "#d46a6a"));
    text->setText(active ? active_text : inactive_text);
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
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(4);

    auto* name_label = new QLabel(name, card);
    name_label->setObjectName("metricName");
    value_label = new QLabel(QStringLiteral("--"), card);
    value_label->setObjectName("metricValue");

    layout->addWidget(name_label);
    layout->addWidget(value_label);
    return card;
}

QLabel* CreateGroupCaption(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName("groupCaption");
    return label;
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

QString FormatNumber(float value)
{
    return QString::number(value, 'f', 1);
}

QString DefaultLogoPath()
{
    return QStringLiteral("qt/assets/log.png");
}

QStringList DefaultAutoGrabTestImages()
{
    return {
        QStringLiteral("/home/cll/桌面/dev/Unordered_scraping-main/asset/20260331_090339_757.jpg"),
        QStringLiteral("/home/cll/桌面/dev/Unordered_scraping-main/asset/20260331_090343_106.jpg"),
        QStringLiteral("/home/cll/桌面/dev/Unordered_scraping-main/asset/2.jpeg"),
        QStringLiteral("/home/cll/桌面/dev/Unordered_scraping-main/asset/3.jpeg"),
        QStringLiteral("/home/cll/桌面/dev/Unordered_scraping-main/asset/1.jpeg"),
        QStringLiteral("/home/cll/桌面/dev/Unordered_scraping-main/asset/3.jpeg"),
    };
}

QIcon CreateAvatarIcon(const QSize& size, const QColor& color)
{
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);

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
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);

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
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color, 1.9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(QPointF(size.width() * 0.24, size.height() * 0.68),
                     QPointF(size.width() * 0.76, size.height() * 0.68));
    return QIcon(pixmap);
}

QIcon CreateMaximizeIcon(const QSize& size, const QColor& color)
{
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);

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
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);

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
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color, 1.9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(QPointF(size.width() * 0.28, size.height() * 0.28),
                     QPointF(size.width() * 0.72, size.height() * 0.72));
    painter.drawLine(QPointF(size.width() * 0.72, size.height() * 0.28),
                     QPointF(size.width() * 0.28, size.height() * 0.72));
    return QIcon(pixmap);
}

QString BuildModelStatusText(const GraspWorkflow& workflow)
{
    return QStringLiteral("模型状态：OBB %1 / SEG %2")
        .arg(workflow.isObbModelLoaded() ? QStringLiteral("已加载") : QStringLiteral("未加载"))
        .arg(workflow.isSegModelLoaded() ? QStringLiteral("已加载") : QStringLiteral("未加载"));
}

int ClampSidebarWidth(int width)
{
    return qBound(kSidebarMinWidth, width, kSidebarMaxWidth);
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

QFrame* CreateStaticMetricCell(const QString& name, const QString& value, QWidget* parent)
{
    auto* card = new QFrame(parent);
    card->setObjectName("compactMetricCell");

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(3);

    auto* name_label = new QLabel(name, card);
    name_label->setObjectName("compactMetricName");
    auto* value_label = new QLabel(value, card);
    value_label->setObjectName("compactMetricValue");
    value_label->setWordWrap(true);

    layout->addWidget(name_label);
    layout->addWidget(value_label);
    return card;
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
    : QMainWindow(parent)
    , robot_controller_(this)
{
    auto_grab_test_images_ = DefaultAutoGrabTestImages();
    setupUi();
    applyStyles();
    bindActions();
    refreshInfoPanel();
    refreshTargetTable();
    refreshDeviceStatus();
    refreshRuntimeStrip();
    setEmptyPreviewMessage(QStringLiteral("请选择图片或打开相机"));
    updateStatusMessage(QStringLiteral("界面已就绪，先在工程设置中同时加载 OBB 和 SEG engine。"));
}

GraspMainWindow::~GraspMainWindow()
{
    stopAutoGrabCycle(false);
    workflow_.stopCamera();
}

void GraspMainWindow::setInitialEnginePaths(const QString& obb_engine_path, const QString& seg_engine_path)
{
    obb_engine_path_ = obb_engine_path;
    seg_engine_path_ = seg_engine_path;
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
    repositionSidePanelToggle();
    renderCurrentFrame();
}

void GraspMainWindow::closeEvent(QCloseEvent* event)
{
    stopAutoGrabCycle(false);
    workflow_.stopCamera();
    QMainWindow::closeEvent(event);
}

void GraspMainWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);

    if (event->type() == QEvent::WindowStateChange) {
        syncWindowControlButtons();
    }
}

void GraspMainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    syncWindowControlButtons();
    repositionSidePanelToggle();

    if (initial_models_attempted_) {
        return;
    }
    initial_models_attempted_ = true;

    if (obb_engine_path_.trimmed().isEmpty() || seg_engine_path_.trimmed().isEmpty()) {
        return;
    }

    loadModelsFromPaths(obb_engine_path_, seg_engine_path_, true, true);
}

void GraspMainWindow::setupUi()
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    resize(1360, 820);
    setMinimumSize(1080, 680);
    setWindowTitle(QStringLiteral("截止阀无序抓取上料视觉检测系统"));

    central_panel_ = new QWidget(this);
    setCentralWidget(central_panel_);
    central_panel_->setMouseTracking(true);
    central_panel_->installEventFilter(this);

    auto* root_layout = new QVBoxLayout(central_panel_);
    root_layout->setContentsMargins(8, 8, 8, 10);
    root_layout->setSpacing(8);

    buildTopBar(root_layout);

    main_splitter_ = new QSplitter(Qt::Horizontal, this);
    main_splitter_->setChildrenCollapsible(false);
    main_splitter_->setHandleWidth(8);
    root_layout->addWidget(main_splitter_, 1);

    buildDisplayPanel(main_splitter_);
    buildSidePanel(main_splitter_);
    main_splitter_->setStretchFactor(0, 5);
    main_splitter_->setStretchFactor(1, 1);

    sidebar_toggle_button_ = new QToolButton(central_panel_);
    sidebar_toggle_button_->setObjectName("sideRailToggleButton");
    sidebar_toggle_button_->setCursor(Qt::PointingHandCursor);
    sidebar_toggle_button_->setFixedSize(kSidebarToggleWidth, kSidebarToggleHeight);
    sidebar_toggle_button_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    sidebar_toggle_button_->raise();

    expanded_sidebar_width_ = ClampSidebarWidth(kSidebarDefaultWidth);
    side_panel_expanded_ = true;
    syncSidePanelToggleButton();
    main_splitter_->setSizes({ qMax(0, width() - expanded_sidebar_width_), expanded_sidebar_width_ });
    repositionSidePanelToggle();
}

void GraspMainWindow::buildTopBar(QVBoxLayout* root_layout)
{
    top_bar_ = new QFrame(this);
    top_bar_->setObjectName("topBar");
    top_bar_->setFixedHeight(56);
    top_bar_->setCursor(Qt::OpenHandCursor);
    top_bar_->installEventFilter(this);

    auto* top_layout = new QGridLayout(top_bar_);
    top_layout->setContentsMargins(14, 6, 14, 6);
    top_layout->setHorizontalSpacing(10);
    top_layout->setVerticalSpacing(0);

    auto* left_widget = new QWidget(top_bar_);
    auto* left_layout = new QHBoxLayout(left_widget);
    left_layout->setContentsMargins(0, 0, 0, 0);
    left_layout->setSpacing(10);
    left_widget->setAttribute(Qt::WA_TransparentForMouseEvents);

    logo_label_ = new QLabel(top_bar_);
    logo_label_->setObjectName("logoLabel");
    logo_label_->setFixedSize(42, 42);
    logo_label_->setAlignment(Qt::AlignCenter);
    const QPixmap logo_pixmap(DefaultLogoPath());
    if (!logo_pixmap.isNull()) {
        logo_label_->setPixmap(logo_pixmap.scaled(28, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        logo_label_->setText(QStringLiteral("L"));
    }

    brand_label_ = new QLabel(QStringLiteral("TankEye1.1"), top_bar_);
    brand_label_->setObjectName("brandLabel");

    left_layout->addWidget(logo_label_);
    left_layout->addWidget(brand_label_);

    title_label_ = new QLabel(QStringLiteral("截止阀无序抓取上料视觉检测系统"), top_bar_);
    title_label_->setObjectName("titleLabel");
    title_label_->setAlignment(Qt::AlignCenter);
    title_label_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    title_label_->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto* right_widget = new QWidget(top_bar_);
    auto* right_layout = new QHBoxLayout(right_widget);
    right_layout->setContentsMargins(0, 0, 0, 0);
    right_layout->setSpacing(6);

    const QSize icon_size(18, 18);
    const QSize button_size(34, 34);
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
    right_layout->addSpacing(2);
    right_layout->addWidget(minimize_window_button_);
    right_layout->addWidget(maximize_window_button_);
    right_layout->addWidget(close_window_button_);

    top_layout->addWidget(left_widget, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
    top_layout->addWidget(title_label_, 0, 1);
    top_layout->addWidget(right_widget, 0, 2, Qt::AlignRight | Qt::AlignVCenter);
    top_layout->setColumnStretch(0, 1);
    top_layout->setColumnStretch(1, 2);
    top_layout->setColumnStretch(2, 1);

    root_layout->addWidget(top_bar_, 0);
    syncWindowControlButtons();
}

void GraspMainWindow::buildDisplayPanel(QSplitter* splitter)
{
    auto* display_panel = new QFrame(this);
    display_panel->setObjectName("displayPanel");
    display_panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* display_layout = new QVBoxLayout(display_panel);
    display_layout->setContentsMargins(16, 12, 16, 16);
    display_layout->setSpacing(12);

    buildRuntimeStrip(display_layout);

    image_label_ = new QLabel(this);
    image_label_->setMinimumSize(480, 320);
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
    runtime_layout->setContentsMargins(10, 8, 10, 8);
    runtime_layout->setSpacing(8);

    runtime_mode_label_ = CreateRuntimeBadge("runtimeBadge");
    runtime_state_label_ = CreateRuntimeBadge("runtimeStateBadge");
    runtime_obb_count_label_ = CreateRuntimeBadge("runtimeBadge");
    runtime_seg_count_label_ = CreateRuntimeBadge("runtimeBadge");
    runtime_stage_time_label_ = CreateRuntimeBadge("runtimeBadge");
    runtime_total_time_label_ = CreateRuntimeBadge("runtimeBadge");

    runtime_layout->addWidget(runtime_mode_label_, 0);
    runtime_layout->addWidget(runtime_state_label_, 0);
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
    side_panel_->setMinimumWidth(kSidebarMinWidth);
    side_panel_->setMaximumWidth(kSidebarMaxWidth);
    side_panel_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    auto* side_layout = new QVBoxLayout(side_panel_);
    side_layout->setContentsMargins(16, 16, 16, 16);
    side_layout->setSpacing(0);

    side_pages_ = new QStackedWidget(side_panel_);

    detail_page_ = new QWidget(side_pages_);
    auto* detail_layout = new QVBoxLayout(detail_page_);
    detail_layout->setContentsMargins(0, 0, 0, 0);
    detail_layout->setSpacing(14);
    buildResultSection(detail_layout);
    buildStatusSection(detail_layout);
    buildActionSection(detail_layout);
    detail_layout->addStretch();
    side_pages_->addWidget(detail_page_);

    target_list_page_ = new QWidget(side_pages_);
    auto* target_list_layout = new QVBoxLayout(target_list_page_);
    target_list_layout->setContentsMargins(0, 0, 0, 0);
    target_list_layout->setSpacing(14);
    buildTargetListSection(target_list_layout);
    side_pages_->addWidget(target_list_page_);

    side_pages_->setCurrentWidget(detail_page_);
    side_layout->addWidget(side_pages_);

    splitter->addWidget(side_panel_);
}

void GraspMainWindow::buildResultSection(QVBoxLayout* side_layout)
{
    side_layout->addWidget(CreateSectionTitle(QStringLiteral("当前目标参数")));

    auto* overview_card = new QFrame(this);
    overview_card->setObjectName("heroInfoCard");
    auto* overview_layout = new QVBoxLayout(overview_card);
    overview_layout->setContentsMargins(14, 14, 14, 14);
    overview_layout->setSpacing(12);
    overview_layout->addWidget(CreateGroupCaption(QStringLiteral("当前主目标"), overview_card));

    auto* metrics = new QHBoxLayout();
    metrics->setSpacing(8);
    metrics->addWidget(CreateMetricCell(QStringLiteral("X"), x_value_label_, overview_card));
    metrics->addWidget(CreateMetricCell(QStringLiteral("Y"), y_value_label_, overview_card));
    metrics->addWidget(CreateMetricCell(QStringLiteral("角度"), angle_value_label_, overview_card));
    overview_layout->addLayout(metrics);

    side_layout->addWidget(overview_card);
}

void GraspMainWindow::buildStatusSection(QVBoxLayout* side_layout)
{
    side_layout->addWidget(CreateSectionTitle(QStringLiteral("系统状态")));

    auto* status_card = new QFrame(this);
    status_card->setObjectName("infoCard");
    auto* status_layout = new QVBoxLayout(status_card);
    status_layout->setContentsMargins(12, 12, 12, 12);
    status_layout->setSpacing(8);

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
        layout->setContentsMargins(10, 8, 10, 8);
        layout->setSpacing(8);
        auto* label = new QLabel(name, container);
        label->setObjectName("statusStackName");
        layout->addWidget(label);
        layout->addWidget(dot);
        layout->addWidget(value);
        layout->addStretch();
        status_layout->addWidget(container);
    };

    add_status_item(QStringLiteral("相机"), camera_status_dot_, camera_status_value_);
    add_status_item(QStringLiteral("OBB"), obb_status_dot_, obb_status_value_);
    add_status_item(QStringLiteral("SEG"), seg_status_dot_, seg_status_value_);
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
    auto_grab_button_ = new QPushButton(QStringLiteral("自动抓取"), this);
    stop_button_ = new QPushButton(QStringLiteral("停止检测"), this);
    target_list_button_ = new QPushButton(QStringLiteral("多目标列表"), this);
    engineering_button_ = new QPushButton(QStringLiteral("工程设置"), this);
    load_image_button_->setMinimumHeight(40);
    open_camera_button_->setMinimumHeight(40);
    start_button_->setMinimumHeight(46);
    auto_grab_button_->setMinimumHeight(46);
    stop_button_->setMinimumHeight(46);
    target_list_button_->setMinimumHeight(36);
    engineering_button_->setMinimumHeight(36);
    target_list_button_->setObjectName("secondaryActionButton");
    engineering_button_->setObjectName("secondaryActionButton");

    auto* input_group = new QFrame(this);
    input_group->setObjectName("controlGroupCard");
    auto* input_layout = new QVBoxLayout(input_group);
    input_layout->setContentsMargins(12, 12, 12, 12);
    input_layout->setSpacing(8);
    input_layout->addWidget(CreateGroupCaption(QStringLiteral("输入来源"), input_group));
    input_layout->addWidget(image_path_label_);
    auto* input_buttons = new QGridLayout();
    input_buttons->setHorizontalSpacing(8);
    input_buttons->setVerticalSpacing(8);
    input_buttons->addWidget(load_image_button_, 0, 0);
    input_buttons->addWidget(open_camera_button_, 0, 1);
    input_layout->addLayout(input_buttons);

    auto* detect_group = new QFrame(this);
    detect_group->setObjectName("controlGroupCard");
    auto* detect_layout = new QVBoxLayout(detect_group);
    detect_layout->setContentsMargins(12, 12, 12, 12);
    detect_layout->setSpacing(8);
    detect_layout->addWidget(CreateGroupCaption(QStringLiteral("检测控制"), detect_group));
    auto* detect_buttons = new QGridLayout();
    detect_buttons->setHorizontalSpacing(8);
    detect_buttons->setVerticalSpacing(8);
    detect_buttons->addWidget(start_button_, 0, 0);
    detect_buttons->addWidget(auto_grab_button_, 0, 1);
    detect_buttons->addWidget(stop_button_, 1, 0, 1, 2);
    detect_layout->addLayout(detect_buttons);
    auto* manage_buttons = new QHBoxLayout();
    manage_buttons->setSpacing(8);
    manage_buttons->addStretch();
    manage_buttons->addWidget(target_list_button_, 0);
    manage_buttons->addWidget(engineering_button_, 0);
    detect_layout->addLayout(manage_buttons);

    side_layout->addWidget(input_group);
    side_layout->addWidget(detect_group);
}

void GraspMainWindow::buildTargetListSection(QVBoxLayout* side_layout)
{
    auto* header_layout = new QHBoxLayout();
    header_layout->setSpacing(8);

    target_list_back_button_ = new QPushButton(QStringLiteral("返回"), this);
    target_list_back_button_->setMinimumHeight(36);
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
    target_list_content_layout_->setSpacing(10);
    target_list_scroll_area_->setWidget(target_list_content_);

    side_layout->addWidget(target_list_scroll_area_, 1);
}

void GraspMainWindow::applyStyles()
{
    setStyleSheet(
        "QMainWindow { background: #0b1118; }"
        "#topBar { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #2c2827, stop:1 #2a2e31);"
        " border: 1px solid #373b3f; border-radius: 14px; }"
        "#displayPanel { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #111b25, stop:0.55 #0d141d, stop:1 #162433);"
        " border: 1px solid #294055; border-radius: 24px; }"
        "#sidePanel { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #172330, stop:1 #101821);"
        " border: 1px solid #2a3f53; border-radius: 24px; }"
        "#runtimeBar { background: rgba(255,255,255,0.04); border: 1px solid #2d4458; border-radius: 14px; }"
        "#imageViewport { background: #060b10; border-radius: 22px; border: 1px solid #34506a; color: #d7e0e8;"
        " font-size: 22px; font-weight: 600; }"
        "#logoLabel { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #f6f6f7, stop:1 #d9dde2);"
        " border: 1px solid #898f97; border-radius: 21px; padding: 2px; }"
        "#brandLabel { color: #ece4dc; font-size: 17px; font-weight: 500; }"
        "#titleLabel { color: #f3eee8; font-size: 20px; font-weight: 800; }"
        "#sectionTitle { color: #eef5fb; font-size: 18px; font-weight: 700; padding-top: 2px; }"
        "#infoCard { background: rgba(255,255,255,0.05); border: 1px solid #2f465b; border-radius: 16px; }"
        "#heroInfoCard { background: rgba(255,255,255,0.06); border: 1px solid #355167; border-radius: 18px; }"
        "#heroSideCard { background: rgba(255,255,255,0.03); border: 1px solid #30495f; border-radius: 14px; }"
        "#groupCaption { color: #8fa9bc; font-size: 12px; font-weight: 600; }"
        "#heroPrimaryValue { color: #f5f9fd; font-size: 24px; font-weight: 800; }"
        "#heroStateValue { color: #f0f7ff; font-size: 18px; font-weight: 700; }"
        "#heroAssistValue { color: #c4d4e1; font-size: 15px; font-weight: 600; }"
        "#metricCell { background: rgba(255,255,255,0.04); border: 1px solid #365066; border-radius: 12px; }"
        "#metricName { color: #96acc0; font-size: 12px; }"
        "#metricValue { color: #f4f8fc; font-size: 20px; font-weight: 700; }"
        "#compactMetricCell { background: rgba(255,255,255,0.035); border: 1px solid #30495f; border-radius: 12px; }"
        "#compactMetricName { color: #8ea3b4; font-size: 11px; }"
        "#compactMetricValue { color: #eef6fc; font-size: 16px; font-weight: 700; }"
        "#runtimeBadge { background: rgba(255,255,255,0.05); color: #c4d4e1; border: 1px solid #2e4a60; border-radius: 10px; padding: 4px 10px; font-size: 12px; }"
        "#runtimeStateBadge { background: rgba(34,115,178,0.18); color: #eef7ff; border: 1px solid #4b8fc5; border-radius: 10px; padding: 4px 12px; font-size: 12px; font-weight: 700; }"
        "#statusPill { background: rgba(255,255,255,0.03); border: 1px solid #2d4458; border-radius: 12px; }"
        "#statusStackItem { background: rgba(255,255,255,0.03); border: 1px solid #30495f; border-radius: 12px; }"
        "#statusStackName { color: #dbe7f1; font-size: 14px; font-weight: 600; }"
        "#controlGroupCard { background: rgba(255,255,255,0.04); border: 1px solid #30495f; border-radius: 16px; }"
        "#pathHintLabel { color: #9eb2c3; background: rgba(255,255,255,0.025); border: 1px solid #2b4256; border-radius: 10px; padding: 8px 10px; font-size: 12px; }"
        "#targetListScrollArea, #targetListScrollContent { background: transparent; border: none; }"
        "QLabel { color: #b7c7d6; }"
        "QLineEdit { background: rgba(255,255,255,0.06); color: #f3f8fc; border: 1px solid #395268; border-radius: 10px; padding: 8px 10px; }"
        "QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #1d9bf0, stop:1 #1367c8); color: white; border: none; border-radius: 12px; padding: 11px 14px;"
        " font-weight: 600; }"
        "QPushButton:hover { background: #1578cb; }"
        "QPushButton:disabled { background: #516576; color: #c9d6e1; }"
        "QPushButton#secondaryActionButton { background: rgba(255,255,255,0.06); color: #ecf4fa; border: 1px solid #36546b; }"
        "QPushButton#secondaryActionButton:hover { background: rgba(255,255,255,0.12); }"
        "QToolButton { color: #d6e6f3; background: rgba(255,255,255,0.03); border: 1px solid #2d4458; border-radius: 10px; padding: 8px 10px; text-align: left; }"
        "QToolButton:checked { background: rgba(34,115,178,0.18); border-color: #4b8fc5; }"
        "QToolButton[topBarButton=\"true\"], QToolButton[windowButton=\"true\"] {"
        " background: transparent; border: 1px solid transparent; border-radius: 17px; padding: 0px; }"
        "QToolButton[topBarButton=\"true\"]:hover, QToolButton[windowButton=\"true\"]:hover {"
        " background: rgba(255,255,255,0.08); border-color: rgba(236,228,220,0.20); }"
        "QToolButton[topBarButton=\"true\"]:pressed, QToolButton[windowButton=\"true\"]:pressed {"
        " background: rgba(255,255,255,0.14); }"
        "QToolButton[closeWindowButton=\"true\"] {"
        " background: transparent; border: 1px solid transparent; border-radius: 17px; padding: 0px; }"
        "QToolButton[closeWindowButton=\"true\"]:hover {"
        " background: #d65757; border-color: #ea7d7d; }"
        "QToolButton[closeWindowButton=\"true\"]:pressed {"
        " background: #b53f3f; }"
        "#sideRailToggleButton { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #2269bf, stop:1 #164b88);"
        " color: #f1f7ff; border: 1px solid #4788d5; border-top-left-radius: 12px; border-bottom-left-radius: 12px;"
        " border-top-right-radius: 6px; border-bottom-right-radius: 6px; font-size: 16px; font-weight: 800; padding: 0px; }"
        "#sideRailToggleButton:hover { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #2b78d4, stop:1 #1a589f); }"
        "QSplitter::handle { background: transparent; }"
        "QSplitter::handle:hover { background: rgba(61, 119, 174, 0.25); }");
}

void GraspMainWindow::bindActions()
{
    connect(load_image_button_, &QPushButton::clicked, this, [this]() { loadImage(); });
    connect(open_camera_button_, &QPushButton::clicked, this, [this]() { openCamera(); });
    connect(start_button_, &QPushButton::clicked, this, [this]() { startDetection(); });
    connect(auto_grab_button_, &QPushButton::clicked, this, [this]() { startAutoGrabCycle(); });
    connect(stop_button_, &QPushButton::clicked, this, [this]() { stopDetection(); });
    connect(target_list_button_, &QPushButton::clicked, this, [this]() { showTargetListPage(); });
    connect(target_list_back_button_, &QPushButton::clicked, this, [this]() { showDetailPage(); });
    connect(engineering_button_, &QPushButton::clicked, this, [this]() { openEngineeringSettings(); });
    connect(user_button_, &QToolButton::clicked, this, [this]() { updateStatusMessage(QStringLiteral("用户中心待接入。"), 3000); });
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
    connect(main_splitter_, &QSplitter::splitterMoved, this, [this](int, int) {
        if (!side_panel_expanded_ || !side_panel_) {
            repositionSidePanelToggle();
            return;
        }

        const int current_width = side_panel_->width();
        const int clamped_width = ClampSidebarWidth(current_width);
        if (clamped_width != current_width) {
            const QList<int> current_sizes = main_splitter_->sizes();
            if (current_sizes.size() >= 2) {
                main_splitter_->setSizes({ qMax(0, current_sizes[0] + current_sizes[1] - clamped_width), clamped_width });
            }
        }
        expanded_sidebar_width_ = ClampSidebarWidth(side_panel_->width());
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
    maximize_window_button_->setToolTip(isMaximized() ? QStringLiteral("还原窗口")
                                                      : QStringLiteral("最大化"));
}

void GraspMainWindow::syncSidePanelToggleButton()
{
    if (!sidebar_toggle_button_) {
        return;
    }

    sidebar_toggle_button_->setText(side_panel_expanded_ ? ">" : "<");
    sidebar_toggle_button_->setToolTip(side_panel_expanded_ ? QStringLiteral("收起右侧侧栏")
                                                            : QStringLiteral("展开右侧侧栏"));
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
        expanded_sidebar_width_ = ClampSidebarWidth(expanded_sidebar_width_);
        side_panel_->setMinimumWidth(kSidebarMinWidth);
        side_panel_->setMaximumWidth(kSidebarMaxWidth);
        side_panel_->show();
        if (main_splitter_->handle(1)) {
            main_splitter_->handle(1)->show();
        }

        const int total_width = qMax(0, main_splitter_->width() - main_splitter_->handleWidth());
        if (total_width > 0) {
            const int sidebar_width = qMin(expanded_sidebar_width_, total_width);
            main_splitter_->setSizes({ qMax(0, total_width - sidebar_width), sidebar_width });
        } else {
            main_splitter_->setSizes({ qMax(0, width() - expanded_sidebar_width_), expanded_sidebar_width_ });
        }
    } else {
        main_splitter_->setSizes({ qMax(0, main_splitter_->width()), 0 });
        side_panel_->hide();
        if (main_splitter_->handle(1)) {
            main_splitter_->handle(1)->hide();
        }
    }

    repositionSidePanelToggle();
}

void GraspMainWindow::repositionSidePanelToggle()
{
    if (!sidebar_toggle_button_ || !central_panel_) {
        return;
    }

    const int x = qMax(0, central_panel_->width() - sidebar_toggle_button_->width() - 2);
    const int y = qMax(70, (central_panel_->height() - sidebar_toggle_button_->height()) / 2);
    sidebar_toggle_button_->move(x, y);
    sidebar_toggle_button_->raise();
}

void GraspMainWindow::openEngineeringSettings()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("工程设置"));
    dialog.setModal(true);
    dialog.resize(760, 220);
    dialog.setStyleSheet(
        "QDialog { background: #f4f6f8; }"
        "QLabel { color: #111111; }"
        "QLineEdit { background: #ffffff; color: #111111; border: 1px solid #9aa9b7; border-radius: 10px; padding: 8px 10px; }"
        "QPushButton { background: #ffffff; color: #111111; border: 1px solid #9aa9b7; border-radius: 10px; padding: 9px 14px; font-weight: 600; }"
        "QPushButton:hover { background: #e9eef3; }");

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto* obb_path_edit = new QLineEdit(obb_engine_path_, &dialog);
    obb_path_edit->setPlaceholderText(QStringLiteral("选择 OBB engine 文件"));
    auto* seg_path_edit = new QLineEdit(seg_engine_path_, &dialog);
    seg_path_edit->setPlaceholderText(QStringLiteral("选择 SEG engine 文件"));

    const auto create_path_row = [&dialog](const QString& title, QLineEdit* path_edit, const QString& dialog_title) {
        auto* row = new QHBoxLayout();
        auto* title_label = new QLabel(title, &dialog);
        title_label->setMinimumWidth(90);
        auto* browse_button = new QPushButton(QStringLiteral("浏览"), &dialog);
        row->addWidget(title_label);
        row->addWidget(path_edit, 1);
        row->addWidget(browse_button);

        QObject::connect(browse_button, &QPushButton::clicked, &dialog, [&dialog, path_edit, dialog_title]() {
            const QString path = QFileDialog::getOpenFileName(&dialog,
                                                              dialog_title,
                                                              path_edit->text(),
                                                              QStringLiteral("TensorRT Engine (*.engine);;All Files (*)"));
            if (!path.isEmpty()) {
                path_edit->setText(path);
            }
        });

        return row;
    };

    auto* status_label = new QLabel(BuildModelStatusText(workflow_), &dialog);
    auto* load_button = new QPushButton(QStringLiteral("加载双模型"), &dialog);

    layout->addLayout(create_path_row(QStringLiteral("OBB Engine"), obb_path_edit, QStringLiteral("选择 OBB Engine")));
    layout->addLayout(create_path_row(QStringLiteral("SEG Engine"), seg_path_edit, QStringLiteral("选择 SEG Engine")));

    auto* bottom_row = new QHBoxLayout();
    bottom_row->addWidget(status_label, 1);
    bottom_row->addWidget(load_button);
    layout->addLayout(bottom_row);

    connect(load_button, &QPushButton::clicked, &dialog, [&]() {
        const QString obb_path = obb_path_edit->text().trimmed();
        const QString seg_path = seg_path_edit->text().trimmed();
        if (!loadModelsFromPaths(obb_path, seg_path, true, true)) {
            status_label->setText(BuildModelStatusText(workflow_));
            return;
        }

        status_label->setText(BuildModelStatusText(workflow_));
        dialog.accept();
    });

    dialog.exec();
}

void GraspMainWindow::loadImage()
{
    const QString path = QFileDialog::getOpenFileName(this,
                                                      QStringLiteral("选择图片"),
                                                      currentImagePath(),
                                                      QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp);;All Files (*)"));
    if (path.isEmpty()) {
        return;
    }

    stopAutoGrabCycle(false);
    loadImageFromPath(path, true);
}

void GraspMainWindow::openCamera()
{
    stopAutoGrabCycle(false);
    workflow_.stopCamera();
    clearResults();

    if (!workflow_.areModelsLoaded()) {
        QMessageBox::warning(this,
                             QStringLiteral("模型未加载"),
                             QStringLiteral("请先在工程设置中同时加载 OBB 和 SEG engine。"));
        refreshDeviceStatus();
        refreshRuntimeStrip();
        return;
    }

    input_mode_ = InputMode::Camera;
    const bool started = workflow_.startCamera(
        [this](const Mat& frame, const FrameInferenceResult& result) {
            Mat frame_copy = frame.clone();
            QMetaObject::invokeMethod(
                this,
                [this, frame_copy, result]() {
                    setFrameAndResult(frame_copy, result);
                    updateStatusMessage(QStringLiteral("相机联合检测中..."), 1000);
                },
                Qt::QueuedConnection);
        },
        [this](const string& error_message) {
            QMetaObject::invokeMethod(
                this,
                [this, error_message]() {
                    refreshDeviceStatus();
                    refreshRuntimeStrip();
                    QMessageBox::critical(this, QStringLiteral("相机错误"), QString::fromStdString(error_message));
                    updateStatusMessage(QStringLiteral("相机检测已停止。"));
                },
                Qt::QueuedConnection);
        });

    refreshDeviceStatus();
    refreshRuntimeStrip();
    if (started) {
        setEmptyPreviewMessage(QStringLiteral("正在打开默认相机..."));
        updateStatusMessage(QStringLiteral("默认相机已启动，正在执行 OBB+SEG 联合检测。"));
    }
}

void GraspMainWindow::startDetection()
{
    stopAutoGrabCycle(false);

    if (!workflow_.areModelsLoaded()) {
        QMessageBox::warning(this,
                             QStringLiteral("模型未加载"),
                             QStringLiteral("请先在工程设置中同时加载 OBB 和 SEG engine。"));
        return;
    }

    if (input_mode_ == InputMode::Camera) {
        if (!workflow_.isCameraRunning()) {
            openCamera();
        }
        return;
    }

    if (current_frame_.empty()) {
        QMessageBox::information(this, QStringLiteral("没有输入"), QStringLiteral("请先加载图片，或打开相机。"));
        return;
    }

    FrameInferenceResult result;
    string error_message;
    if (!workflow_.runImage(current_frame_, result, &error_message)) {
        QMessageBox::critical(this, QStringLiteral("检测失败"), QString::fromStdString(error_message));
        return;
    }

    setFrameAndResult(current_frame_, result);
    updateStatusMessage(result.detections.empty() && result.segments.empty()
                            ? QStringLiteral("未检测到 OBB 或 SEG 结果。")
                            : QStringLiteral("图片联合检测完成。"),
                        5000);
}

void GraspMainWindow::stopDetection()
{
    stopAutoGrabCycle();
    workflow_.stopCamera();
    refreshDeviceStatus();
    refreshRuntimeStrip();
    updateStatusMessage(QStringLiteral("检测已停止。"), 4000);
}

void GraspMainWindow::startAutoGrabCycle()
{
    cout << "[AutoGrab] start requested" << endl;
    if (!workflow_.areModelsLoaded()) {
        cout << "[AutoGrab] rejected: models are not loaded" << endl;
        QMessageBox::warning(this,
                             QStringLiteral("模型未加载"),
                             QStringLiteral("请先在工程设置中同时加载 OBB 和 SEG engine。"));
        return;
    }

    if (input_mode_ == InputMode::Camera) {
        cout << "[AutoGrab] input mode: camera; every robot-home callback will re-detect the latest camera frame" << endl;
        if (!workflow_.isCameraRunning()) {
            openCamera();
        }
    } else if (current_frame_.empty()) {
        cout << "[AutoGrab] no image loaded, trying first built-in test image" << endl;
        if (!auto_grab_test_images_.empty() && loadImageFromPath(auto_grab_test_images_.front(), false)) {
            auto_grab_test_image_index_ = 0;
            cout << "[AutoGrab] loaded built-in test image: "
                 << auto_grab_test_images_.front().toStdString() << endl;
        } else {
            cout << "[AutoGrab] rejected: no input image and built-in test image failed" << endl;
            QMessageBox::information(this, QStringLiteral("没有输入"), QStringLiteral("请先加载图片，或打开相机。"));
            return;
        }
    } else {
        const QString current_path = image_path_label_->text().trimmed();
        auto_grab_test_image_index_ = auto_grab_test_images_.indexOf(current_path);
        cout << "[AutoGrab] input mode: image, current path=" << current_path.toStdString()
             << ", test index=" << auto_grab_test_image_index_ << endl;
    }

    auto_grab_active_ = true;
    runOneAutoGrabCycle();
}

void GraspMainWindow::runOneAutoGrabCycle()
{
    if (!auto_grab_active_) {
        cout << "[AutoGrab] skip cycle: auto grab is inactive" << endl;
        return;
    }

    if (!workflow_.areModelsLoaded()) {
        cout << "[AutoGrab] stop: models are not loaded" << endl;
        stopAutoGrabCycle();
        QMessageBox::warning(this,
                             QStringLiteral("模型未加载"),
                             QStringLiteral("请先在工程设置中同时加载 OBB 和 SEG engine。"));
        return;
    }

    if (current_frame_.empty() && input_mode_ == InputMode::Camera) {
        cout << "[AutoGrab] waiting for camera frame" << endl;
        auto_grab_state_ = AutoGrabState::Detecting;
        refreshDeviceStatus();
        refreshRuntimeStrip();
        updateStatusMessage(QStringLiteral("自动检测中，等待相机画面..."));
        QTimer::singleShot(300, this, [this]() { runOneAutoGrabCycle(); });
        return;
    }

    if (current_frame_.empty()) {
        handleAutoGrabNoTarget();
        return;
    }

    auto_grab_state_ = AutoGrabState::Detecting;
    refreshDeviceStatus();
    refreshRuntimeStrip();
    updateStatusMessage(QStringLiteral("自动检测中"));
    cout << "[AutoGrab] detecting frame: " << image_path_label_->text().trimmed().toStdString() << endl;

    FrameInferenceResult result;
    string error_message;
    if (!workflow_.runImage(current_frame_, result, &error_message)) {
        cout << "[AutoGrab] detection failed: " << error_message << endl;
        stopAutoGrabCycle();
        QMessageBox::critical(this, QStringLiteral("检测失败"), QString::fromStdString(error_message));
        return;
    }

    setFrameAndResult(current_frame_, result);
    cout << "[AutoGrab] detection done: candidates=" << current_result_.detections.size()
         << ", segments=" << current_result_.segments.size()
         << ", primary_index=" << current_result_.primary_index
         << ", total_ms=" << current_result_.total_inference_ms << endl;
    const int primary_index = current_result_.primary_index;
    if (primary_index < 0 || primary_index >= static_cast<int>(current_result_.detections.size())) {
        handleAutoGrabNoTarget();
        return;
    }

    const PoseDetection target = current_result_.detections[primary_index];
    cout << "[AutoGrab] primary target: index=" << primary_index
         << ", x=" << target.center_x
         << ", y=" << target.center_y
         << ", angle=" << target.angle_deg
         << ", obb_class=" << target.class_name
         << ", seg_class=" << target.segment_class_name
         << ", conf=" << target.confidence << endl;
    auto_grab_state_ = AutoGrabState::WaitingRobot;
    refreshDeviceStatus();
    refreshRuntimeStrip();
    updateStatusMessage(QStringLiteral("已找到主目标，机械臂执行中"));

    robot_controller_.grabAsync(target, [this]() {
        QMetaObject::invokeMethod(this, [this]() { onRobotReturnedHome(); }, Qt::QueuedConnection);
    });
}

void GraspMainWindow::onRobotReturnedHome()
{
    if (!auto_grab_active_) {
        cout << "[AutoGrab] robot callback ignored: auto grab is inactive" << endl;
        return;
    }

    cout << "[AutoGrab] robot returned home callback received" << endl;
    updateStatusMessage(QStringLiteral("机械臂已回原点，重新检测"));
    if (input_mode_ == InputMode::Image) {
        const bool advanced = advanceAutoGrabTestImage();
        cout << "[AutoGrab] test image advance: " << (advanced ? "yes" : "no")
             << ", current=" << image_path_label_->text().trimmed().toStdString() << endl;
        if (!advanced) {
            finishAutoGrabCycle(QStringLiteral("图片序列已处理完，自动抓取流程结束"));
            return;
        }
    } else if (input_mode_ == InputMode::Camera) {
        cout << "[AutoGrab] camera mode: re-detecting the latest captured frame" << endl;
    }

    QTimer::singleShot(0, this, [this]() { runOneAutoGrabCycle(); });
}

void GraspMainWindow::handleAutoGrabNoTarget()
{
    if (input_mode_ == InputMode::Camera) {
        cout << "[AutoGrab] no grabbable target in this camera frame; waiting for next frame" << endl;
        auto_grab_state_ = AutoGrabState::Detecting;
        refreshDeviceStatus();
        refreshRuntimeStrip();
        updateStatusMessage(QStringLiteral("当前帧无可抓取目标，继续检测下一帧..."), 2000);
        QTimer::singleShot(300, this, [this]() { runOneAutoGrabCycle(); });
        return;
    }

    if (input_mode_ == InputMode::Image) {
        cout << "[AutoGrab] no grabbable target in this image; pausing before next test image" << endl;
        auto_grab_state_ = AutoGrabState::NoTarget;
        refreshDeviceStatus();
        refreshRuntimeStrip();
        updateStatusMessage(QStringLiteral("当前图片无可抓取目标，稍后检测下一张图片..."), 1500);

        QTimer::singleShot(1500, this, [this]() {
            if (!auto_grab_active_) {
                cout << "[AutoGrab] image no-target pause ended, but auto grab is inactive" << endl;
                return;
            }

            const bool advanced = advanceAutoGrabTestImage();
            if (advanced) {
                auto_grab_state_ = AutoGrabState::Detecting;
                refreshDeviceStatus();
                refreshRuntimeStrip();
                updateStatusMessage(QStringLiteral("继续检测下一张图片..."), 1500);
                runOneAutoGrabCycle();
                return;
            }

            finishAutoGrabCycle(QStringLiteral("图片序列已检测完，未找到新的可抓取目标"));
        });
        return;
    }

    finishAutoGrabCycle(QStringLiteral("未检测到可抓取目标，流程结束"));
}

void GraspMainWindow::finishAutoGrabCycle(const QString& message)
{
    cout << "[AutoGrab] finished: " << message.toStdString() << endl;
    auto_grab_active_ = false;
    auto_grab_state_ = AutoGrabState::NoTarget;
    robot_controller_.cancel();
    refreshDeviceStatus();
    refreshRuntimeStrip();
    updateStatusMessage(message, 6000);
}

void GraspMainWindow::stopAutoGrabCycle(bool mark_stopped)
{
    if (!auto_grab_active_ && auto_grab_state_ == AutoGrabState::Idle && !robot_controller_.isBusy()) {
        return;
    }

    cout << (mark_stopped ? "[AutoGrab] stopped by user" : "[AutoGrab] reset/cancelled silently") << endl;
    auto_grab_active_ = false;
    auto_grab_state_ = mark_stopped ? AutoGrabState::Stopped : AutoGrabState::Idle;
    robot_controller_.cancel();
    refreshDeviceStatus();
    refreshRuntimeStrip();
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
    image_path_label_->setText(path);

    Mat image = imread(path.toStdString());
    if (image.empty()) {
        cout << "[Image] load failed: " << path.toStdString() << endl;
        if (notify) {
            QMessageBox::critical(this, QStringLiteral("读取失败"), QStringLiteral("无法读取所选图片。"));
        }
        return false;
    }

    current_frame_ = image;
    cout << "[Image] loaded: " << path.toStdString()
         << " (" << image.cols << "x" << image.rows << ")" << endl;
    clearResults();
    renderCurrentFrame();
    refreshDeviceStatus();
    refreshRuntimeStrip();
    if (notify) {
        updateStatusMessage(QStringLiteral("图片已加载，点击“开始检测”执行 OBB+SEG 联合推理。"));
    }
    return true;
}

bool GraspMainWindow::advanceAutoGrabTestImage()
{
    if (auto_grab_test_images_.empty()) {
        cout << "[AutoGrab] test image advance failed: no test images configured" << endl;
        return false;
    }

    const QString current_path = image_path_label_->text().trimmed();
    int current_index = auto_grab_test_image_index_;
    if (current_index < 0) {
        current_index = auto_grab_test_images_.indexOf(current_path);
    }

    const int next_index = current_index + 1;
    if (next_index < 0 || next_index >= auto_grab_test_images_.size()) {
        cout << "[AutoGrab] test image advance skipped: no next image, current_index="
             << current_index << ", total=" << auto_grab_test_images_.size() << endl;
        return false;
    }

    if (!loadImageFromPath(auto_grab_test_images_[next_index], false)) {
        cout << "[AutoGrab] test image advance failed: cannot load "
             << auto_grab_test_images_[next_index].toStdString() << endl;
        return false;
    }

    auto_grab_test_image_index_ = next_index;
    cout << "[AutoGrab] advanced to test image index " << next_index
         << ": " << auto_grab_test_images_[next_index].toStdString() << endl;
    return true;
}

void GraspMainWindow::renderCurrentFrame()
{
    if (current_frame_.empty()) {
        return;
    }

    Mat display = current_frame_.clone();
    DrawFrameOverlay(display, current_result_, -1, false);

    const QImage image = MatToQImage(display);
    if (image.isNull()) {
        image_label_->setText(QStringLiteral("图像渲染失败"));
        return;
    }

    const QPixmap pixmap = QPixmap::fromImage(image);
    image_label_->setPixmap(pixmap.scaled(image_label_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void GraspMainWindow::refreshInfoPanel()
{
    const int resolved_index = current_result_.primary_index;

    if (resolved_index < 0 || resolved_index >= static_cast<int>(current_result_.detections.size())) {
        x_value_label_->setText(QStringLiteral("--"));
        y_value_label_->setText(QStringLiteral("--"));
        angle_value_label_->setText(QStringLiteral("--"));
        return;
    }

    const auto& detection = current_result_.detections[resolved_index];
    x_value_label_->setText(FormatNumber(detection.center_x));
    y_value_label_->setText(FormatNumber(detection.center_y));
    angle_value_label_->setText(FormatNumber(detection.angle_deg) + QStringLiteral(" deg"));
}

void GraspMainWindow::refreshTargetTable()
{
    if (!target_list_content_layout_ || !target_list_content_) {
        return;
    }

    ClearLayoutItems(target_list_content_layout_);

    if (current_result_.detections.empty()) {
        auto* empty_card = new QLabel(QStringLiteral("当前无检测结果"), target_list_content_);
        empty_card->setObjectName("pathHintLabel");
        empty_card->setWordWrap(true);
        target_list_content_layout_->addWidget(empty_card);
        target_list_content_layout_->addStretch();
        return;
    }

    for (int i = 0; i < static_cast<int>(current_result_.detections.size()); ++i) {
        const auto& detection = current_result_.detections[i];
        auto* card = new QFrame(target_list_content_);
        card->setObjectName("infoCard");
        auto* card_layout = new QVBoxLayout(card);
        card_layout->setContentsMargins(12, 12, 12, 12);
        card_layout->setSpacing(8);

        auto* title_label = new QLabel(QStringLiteral("序号 %1").arg(i + 1), card);
        title_label->setObjectName("statusStackName");
        card_layout->addWidget(title_label);

        auto* metrics_layout = new QHBoxLayout();
        metrics_layout->setSpacing(8);
        metrics_layout->addWidget(CreateStaticMetricCell(QStringLiteral("X"), FormatNumber(detection.center_x), card));
        metrics_layout->addWidget(CreateStaticMetricCell(QStringLiteral("Y"), FormatNumber(detection.center_y), card));
        metrics_layout->addWidget(
            CreateStaticMetricCell(QStringLiteral("角度"), FormatNumber(detection.angle_deg) + QStringLiteral(" deg"), card));
        card_layout->addLayout(metrics_layout);

        target_list_content_layout_->addWidget(card);
    }

    target_list_content_layout_->addStretch();
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

void GraspMainWindow::refreshDeviceStatus()
{
    SetIndicator(camera_status_dot_, camera_status_value_, workflow_.isCameraRunning(),
                 QStringLiteral("在线"), QStringLiteral("待机"));
    SetIndicator(obb_status_dot_, obb_status_value_, workflow_.isObbModelLoaded(),
                 QStringLiteral("已加载"), QStringLiteral("未加载"));
    SetIndicator(seg_status_dot_, seg_status_value_, workflow_.isSegModelLoaded(),
                 QStringLiteral("已加载"), QStringLiteral("未加载"));

    const bool has_input = !current_frame_.empty() || input_mode_ == InputMode::Camera;
    start_button_->setEnabled(workflow_.areModelsLoaded() && has_input);
    auto_grab_button_->setEnabled(workflow_.areModelsLoaded() && has_input && !auto_grab_active_);
    stop_button_->setEnabled(workflow_.isCameraRunning() || auto_grab_active_ || robot_controller_.isBusy());
}

void GraspMainWindow::refreshRuntimeStrip()
{
    const QString mode = input_mode_ == InputMode::Camera ? QStringLiteral("实时")
                                                          : (input_mode_ == InputMode::Image ? QStringLiteral("图片")
                                                                                             : QStringLiteral("待机"));
    runtime_mode_label_->setText(QStringLiteral("模式：%1").arg(mode));

    QString state = QStringLiteral("待机");
    if (auto_grab_state_ == AutoGrabState::Detecting) {
        state = QStringLiteral("自动检测中");
    } else if (auto_grab_state_ == AutoGrabState::WaitingRobot) {
        state = QStringLiteral("机械臂执行中");
    } else if (auto_grab_state_ == AutoGrabState::NoTarget) {
        state = QStringLiteral("无可抓取目标");
    } else if (auto_grab_state_ == AutoGrabState::Stopped) {
        state = QStringLiteral("检测已停止");
    } else if (!workflow_.areModelsLoaded()) {
        state = QStringLiteral("模型未就绪");
    } else if (workflow_.isCameraRunning()) {
        state = QStringLiteral("联合检测中");
    } else if (!current_frame_.empty() && current_result_.total_inference_ms > 0.0) {
        state = (current_result_.detections.empty() && current_result_.segments.empty())
                    ? QStringLiteral("无结果")
                    : QStringLiteral("检测完成");
    } else if (!current_frame_.empty()) {
        state = QStringLiteral("等待执行");
    } else if (input_mode_ == InputMode::Camera) {
        state = QStringLiteral("等待相机");
    } else {
        state = QStringLiteral("等待图像输入");
    }

    runtime_state_label_->setText(QStringLiteral("状态：%1").arg(state));
    runtime_obb_count_label_->setText(QStringLiteral("OBB：%1").arg(current_result_.detections.size()));
    runtime_seg_count_label_->setText(QStringLiteral("SEG：%1").arg(current_result_.segments.size()));

    runtime_stage_time_label_->setText(
        current_result_.total_inference_ms > 0.0
            ? QStringLiteral("阶段耗时：OBB %1 / SEG %2 ms")
                  .arg(QString::number(current_result_.obb_inference_ms, 'f', 1),
                       QString::number(current_result_.seg_inference_ms, 'f', 1))
            : QStringLiteral("阶段耗时：--"));
    runtime_total_time_label_->setText(
        current_result_.total_inference_ms > 0.0
            ? QStringLiteral("总耗时：%1 ms").arg(QString::number(current_result_.total_inference_ms, 'f', 1))
            : QStringLiteral("总耗时：--"));
}

void GraspMainWindow::updateStatusMessage(const QString& message, int timeout_ms)
{
    statusBar()->showMessage(message, timeout_ms);
}

void GraspMainWindow::clearResults()
{
    current_result_ = FrameInferenceResult{};
    refreshInfoPanel();
    refreshTargetTable();
    refreshRuntimeStrip();
}

void GraspMainWindow::setEmptyPreviewMessage(const QString& message)
{
    current_frame_.release();
    image_label_->setPixmap(QPixmap());
    image_label_->setText(message);
    refreshRuntimeStrip();
}

bool GraspMainWindow::loadModelsFromPaths(const QString& obb_engine_path,
                                          const QString& seg_engine_path,
                                          bool show_error_dialog,
                                          bool notify_success)
{
    const QString trimmed_obb_path = obb_engine_path.trimmed();
    const QString trimmed_seg_path = seg_engine_path.trimmed();
    if (trimmed_obb_path.isEmpty() || trimmed_seg_path.isEmpty()) {
        if (show_error_dialog) {
            QMessageBox::warning(this,
                                 QStringLiteral("缺少模型"),
                                 QStringLiteral("请同时选择 OBB 和 SEG engine 文件。"));
        }
        refreshDeviceStatus();
        refreshRuntimeStrip();
        return false;
    }

    workflow_.stopCamera();
    stopAutoGrabCycle(false);

    OBBConfig obb_config;
    SEGConfig seg_config;
    string error_message;
    if (!workflow_.loadModels(trimmed_obb_path.toStdString(),
                              obb_config,
                              trimmed_seg_path.toStdString(),
                              seg_config,
                              &error_message)) {
        refreshDeviceStatus();
        refreshRuntimeStrip();
        if (show_error_dialog) {
            QMessageBox::critical(this, QStringLiteral("模型加载失败"), QString::fromStdString(error_message));
        }
        updateStatusMessage(QStringLiteral("双模型加载失败，请检查 engine 路径。"), 5000);
        return false;
    }

    obb_engine_path_ = trimmed_obb_path;
    seg_engine_path_ = trimmed_seg_path;
    refreshDeviceStatus();
    refreshRuntimeStrip();
    if (notify_success) {
        updateStatusMessage(QStringLiteral("OBB 和 SEG 模型已加载，可以开始联合检测。"), 5000);
    }
    return true;
}

QString GraspMainWindow::currentImagePath() const
{
    const QString path = image_path_label_->text().trimmed();
    if (!path.isEmpty()) {
        const QFileInfo info(path);
        if (info.exists()) {
            return info.isDir() ? info.absoluteFilePath() : info.absolutePath();
        }
    }
    return QStringLiteral("./asset");
}
