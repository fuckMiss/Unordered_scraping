#pragma once

#include "grasp_workflow.h"
#include "robot_controller.h"

#include <QMainWindow>
#include <QPoint>
#include <QStringList>
#include <Qt>

class QFrame;
class QLabel;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QToolButton;
class QHBoxLayout;
class QSplitter;
class QVBoxLayout;
class QResizeEvent;
class QCloseEvent;
class QEvent;
class QShowEvent;

class GraspMainWindow : public QMainWindow
{
public:
    explicit GraspMainWindow(QWidget* parent = nullptr);
    ~GraspMainWindow() override;

    void setInitialEnginePaths(const QString& obb_engine_path, const QString& seg_engine_path);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    enum class InputMode {
        Idle,
        Image,
        Camera
    };

    enum class AutoGrabState {
        Idle,
        Detecting,
        WaitingRobot,
        NoTarget,
        Stopped
    };

    void setupUi();
    void buildTopBar(QVBoxLayout* root_layout);
    void buildDisplayPanel(QSplitter* splitter);
    void buildSidePanel(QSplitter* splitter);
    void buildRuntimeStrip(QVBoxLayout* display_layout);
    void buildResultSection(QVBoxLayout* side_layout);
    void buildStatusSection(QVBoxLayout* side_layout);
    void buildActionSection(QVBoxLayout* side_layout);
    void buildTargetListSection(QVBoxLayout* side_layout);
    void applyStyles();
    void bindActions();
    void syncWindowControlButtons();
    void syncSidePanelToggleButton();
    void toggleSidePanel();
    void applySidePanelState(bool expanded);
    void repositionSidePanelToggle();

    void openEngineeringSettings();
    void loadImage();
    void openCamera();
    void startDetection();
    void stopDetection();
    void startAutoGrabCycle();
    void runOneAutoGrabCycle();
    void onRobotReturnedHome();
    void handleAutoGrabNoTarget();
    void finishAutoGrabCycle(const QString& message);
    void stopAutoGrabCycle(bool mark_stopped = true);

    void setFrameAndResult(const cv::Mat& frame, const FrameInferenceResult& result);
    bool loadImageFromPath(const QString& path, bool notify);
    bool advanceAutoGrabTestImage();
    void renderCurrentFrame();
    void refreshInfoPanel();
    void refreshTargetTable();
    void refreshDeviceStatus();
    void refreshRuntimeStrip();
    void updateStatusMessage(const QString& message, int timeout_ms = 0);
    void showDetailPage();
    void showTargetListPage();
    void clearResults();
    void setEmptyPreviewMessage(const QString& message);
    bool loadModelsFromPaths(const QString& obb_engine_path,
                             const QString& seg_engine_path,
                             bool show_error_dialog,
                             bool notify_success);
    QString currentImagePath() const;

    QWidget* central_panel_ = nullptr;
    QFrame* top_bar_ = nullptr;
    QSplitter* main_splitter_ = nullptr;
    QFrame* side_panel_ = nullptr;
    QLabel* logo_label_ = nullptr;
    QLabel* brand_label_ = nullptr;
    QLabel* title_label_ = nullptr;
    QLabel* image_label_ = nullptr;
    QLabel* runtime_mode_label_ = nullptr;
    QLabel* runtime_state_label_ = nullptr;
    QLabel* runtime_obb_count_label_ = nullptr;
    QLabel* runtime_seg_count_label_ = nullptr;
    QLabel* runtime_stage_time_label_ = nullptr;
    QLabel* runtime_total_time_label_ = nullptr;
    QLabel* image_path_label_ = nullptr;
    QPushButton* load_image_button_ = nullptr;
    QPushButton* open_camera_button_ = nullptr;
    QPushButton* start_button_ = nullptr;
    QPushButton* auto_grab_button_ = nullptr;
    QPushButton* stop_button_ = nullptr;
    QPushButton* engineering_button_ = nullptr;
    QPushButton* target_list_button_ = nullptr;
    QPushButton* target_list_back_button_ = nullptr;
    QToolButton* user_button_ = nullptr;
    QToolButton* settings_icon_button_ = nullptr;
    QToolButton* minimize_window_button_ = nullptr;
    QToolButton* maximize_window_button_ = nullptr;
    QToolButton* close_window_button_ = nullptr;
    QToolButton* sidebar_toggle_button_ = nullptr;
    QStackedWidget* side_pages_ = nullptr;
    QWidget* detail_page_ = nullptr;
    QWidget* target_list_page_ = nullptr;
    QScrollArea* target_list_scroll_area_ = nullptr;
    QWidget* target_list_content_ = nullptr;
    QVBoxLayout* target_list_content_layout_ = nullptr;
    QLabel* camera_status_dot_ = nullptr;
    QLabel* camera_status_value_ = nullptr;
    QLabel* obb_status_dot_ = nullptr;
    QLabel* obb_status_value_ = nullptr;
    QLabel* seg_status_dot_ = nullptr;
    QLabel* seg_status_value_ = nullptr;
    QLabel* x_value_label_ = nullptr;
    QLabel* y_value_label_ = nullptr;
    QLabel* angle_value_label_ = nullptr;

    GraspWorkflow workflow_;
    RobotController robot_controller_;
    InputMode input_mode_ = InputMode::Idle;
    AutoGrabState auto_grab_state_ = AutoGrabState::Idle;
    cv::Mat current_frame_;
    FrameInferenceResult current_result_;
    QString obb_engine_path_;
    QString seg_engine_path_;
    QStringList auto_grab_test_images_;
    int auto_grab_test_image_index_ = -1;
    bool auto_grab_active_ = false;
    bool initial_models_attempted_ = false;
    bool top_bar_dragging_ = false;
    QPoint top_bar_drag_offset_;
    QPoint top_bar_press_global_pos_;
    qreal top_bar_press_ratio_ = 0.5;
    bool top_bar_press_active_ = false;
    bool resizing_frame_ = false;
    bool side_panel_expanded_ = true;
    int expanded_sidebar_width_ = 360;
    Qt::Edges active_resize_edges_;
    QRect resize_start_geometry_;
    QPoint resize_start_global_pos_;
};
