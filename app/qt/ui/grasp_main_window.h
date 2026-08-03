#pragma once

#include "app_config_service.h"
#include "coordinate_transform.h"
#include "grab_limit_evaluator.h"
#include "grasp_workflow.h"
#include "plc_trigger_coordinator.h"
#include "plc_runtime_state.h"
#include "robot_controller.h"
#include "runtime_status_presenter.h"

#include <QFutureWatcher>
#include <QList>
#include <QMainWindow>
#include <QPoint>
#include <QStringList>
#include <Qt>

#include <atomic>
#include <chrono>
#include <memory>

class QFrame;
class QDialog;
class QLabel;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QToolButton;
class QHBoxLayout;
class QGridLayout;
class QSplitter;
class QVBoxLayout;
class QResizeEvent;
class QCloseEvent;
class QEvent;
class QShowEvent;
class QTimer;
class QString;
class EngineeringSettingsDialogController;
struct EngineeringSettingsDraft;

class GraspMainWindow : public QMainWindow
{
    friend class EngineeringSettingsDialogController;

public:
    explicit GraspMainWindow(QWidget* parent = nullptr);
    explicit GraspMainWindow(const AppConfig& app_config, QWidget* parent = nullptr);
    ~GraspMainWindow() override;

    void setInitialModelPaths(const QString& obb_model_path, const QString& seg_model_path);
    void setShowAllDetections(bool show_all_detections);

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

    struct ImageLoadResult {
        QString path;
        QString error_message;
        cv::Mat image;
    };

    struct TargetCard {
        QFrame* card = nullptr;
        QLabel* title_label = nullptr;
        QLabel* image_x_label = nullptr;
        QLabel* image_y_label = nullptr;
        QLabel* machine_x_label = nullptr;
        QLabel* machine_y_label = nullptr;
        QLabel* angle_label = nullptr;
        QLabel* pick_status_label = nullptr;
        QLabel* head_type_label = nullptr;
    };

    void setupUi();
    void buildTopBar(QVBoxLayout* root_layout);
    void buildDisplayPanel(QSplitter* splitter);
    void buildSidePanel(QSplitter* splitter);
    void buildRuntimeStrip(QVBoxLayout* display_layout);
    void buildResultSection(QVBoxLayout* side_layout);
    void buildStatusSection(QVBoxLayout* side_layout);
    void buildActionSection(QVBoxLayout* side_layout);
    void buildFunctionSection(QVBoxLayout* side_layout);
    void buildTargetListSection(QVBoxLayout* side_layout);
    void applyStyles();
    void applyResponsiveLayout(bool force = false);
    void refreshTopBarMetrics();
    void refreshActionButtonMetrics();
    void syncMainSplitterRatio();
    void refreshSidebarCompactMetrics();
    int responsiveSidebarWidth() const;
    void bindActions();
    void syncWindowControlButtons();
    void syncSidePanelToggleButton();
    bool isVisuallyMaximized() const;
    void logHighDpiMetrics(const QString& context) const;
    void toggleSidePanel();
    void applySidePanelState(bool expanded);
    void repositionSidePanelToggle();
    void waitForBackgroundJobs();
    void resetCameraUiCounters();
    void stopPlcLinkState(RuntimeWorkflowState next_state);
    void startPlcPollingState();

    void openEngineeringSettings();
    void showRuntimeLogs();
    void handleUserButtonClicked();
    void showCreateAdminAccountDialog();
    void showAdminLoginDialog();
    bool showAdminResetDialog();
    bool hasAdminAccount() const;
    QString adminUsername() const;
    bool setAdminCredentials(const QString& username, const QString& password, QString* error_message);
    bool changeAdminCredentials(const QString& current_password,
                                const QString& username,
                                const QString& new_password,
                                QString* error_message);
    bool validateAdminCredentials(const QString& username, const QString& password) const;
    QString rememberedAdminPassword() const;
    void saveRememberedAdminPassword(bool remember, const QString& password);
    void setAdminMode(bool enabled);
    void refreshAdminModeUi();
    void loadImage();
    void openCamera();
    void closeCamera();
    void startDetection();
    void stopDetection();
    void togglePlcLinkMode();
    void pollPlcTrigger();
    void finishPlcTriggerPoll(QFutureWatcher<QString>* watcher,
                              const std::shared_ptr<bool>& triggered);
    void processPlcTriggeredFrame();
    void finishPlcTriggeredFrame(QFutureWatcher<QString>* watcher,
                                 const std::shared_ptr<cv::Mat>& frame,
                                 const std::shared_ptr<FrameInferenceResult>& result,
                                 const std::shared_ptr<PlcTriggerProcessResult>& plc_process_result,
                                 const std::chrono::steady_clock::time_point& plc_start);
    void writeCurrentResultToPlc(const QString& context,
                                 bool clear_trigger,
                                 const FrameInferenceResult& result);
    void writePlcTestValues();
    void loadLimitSettings();
    void saveLimitSettings() const;
    void loadCameraSettings();
    void saveCameraSettings() const;
    void loadModelThresholdSettings();
    void saveModelThresholdSettings() const;
    void loadAngleCalibrationSettings();
    void saveAngleCalibrationSettings() const;
    void loadObbPostprocessSettings();
    void saveObbPostprocessSettings() const;
    void loadUiOverlaySettings();
    void saveUiOverlaySettings() const;
    void loadAxisMappingSettings();
    void saveAxisMappingSettings() const;
    void loadAxisCompensationSettings();
    void saveAxisCompensationSettings() const;
    void loadCoordinateTransformSettings();
    bool saveCoordinateTransformSettings(QString* error_message = nullptr) const;
    void applyEngineeringSettingsDraft(const EngineeringSettingsDraft& draft);
    void rebuildCoordinateTransformState();
    QString frontBackAxisLabel() const;
    QString leftRightAxisLabel() const;
    float calibratedAngle(float angle_deg) const;
    PlcOutputConfig plcOutputConfig() const;

    void setFrameAndResult(const cv::Mat& frame, const FrameInferenceResult& result);
    bool loadImageFromPath(const QString& path, bool notify);
    void renderCurrentFrame();
    void refreshInfoPanel();
    void refreshTargetTable();
    void createTargetCard(TargetCard& target_card);
    void refreshTargetCard(TargetCard& target_card, const PoseDetection& detection, int detection_index);
    void refreshRuntimePresentation();
    void refreshResultPresentation(bool render_frame);
    RuntimeStatusSnapshot runtimeStatusSnapshot() const;
    void refreshDeviceStatus();
    void refreshRuntimeStrip();
    void updateStatusMessage(const QString& message, int timeout_ms = 0);
    void showPlcTestResultDialog(const FrameInferenceResult& result);
    void showDetailPage();
    void showTargetListPage();
    void clearResults();
    void setEmptyPreviewMessage(const QString& message);
    void setModelLoadingState(bool loading);
    void loadModelsFromPathsAsync(const QString& obb_model_path,
                                  const QString& seg_model_path,
                                  bool show_error_dialog,
                                  bool notify_success);
    QString currentImagePath() const;

    QWidget* central_panel_ = nullptr;
    std::unique_ptr<EngineeringSettingsDialogController> engineering_settings_dialog_controller_;
    QDialog* engineering_settings_dialog_ = nullptr;
    QFrame* top_bar_ = nullptr;
    QWidget* top_right_controls_ = nullptr;
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
    QLabel* pick_status_value_label_ = nullptr;
    QLabel* head_type_value_label_ = nullptr;
    QLabel* image_path_label_ = nullptr;
    QPushButton* load_image_button_ = nullptr;
    QPushButton* open_camera_button_ = nullptr;
    QPushButton* start_button_ = nullptr;
    QPushButton* plc_link_button_ = nullptr;
    QPushButton* plc_test_button_ = nullptr;
    QPushButton* stop_button_ = nullptr;
    QPushButton* display_mode_button_ = nullptr;
    QPushButton* engineering_button_ = nullptr;
    QPushButton* target_list_button_ = nullptr;
    QPushButton* runtime_log_button_ = nullptr;
    QFrame* input_group_ = nullptr;
    QGridLayout* detect_buttons_layout_ = nullptr;
    QFrame* function_group_ = nullptr;
    QGridLayout* function_layout_ = nullptr;
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
    QLabel* target_empty_label_ = nullptr;
    QList<TargetCard> target_cards_;
    QLabel* camera_status_dot_ = nullptr;
    QLabel* camera_status_value_ = nullptr;
    QLabel* obb_status_dot_ = nullptr;
    QLabel* obb_status_value_ = nullptr;
    QLabel* seg_status_dot_ = nullptr;
    QLabel* seg_status_value_ = nullptr;
    QLabel* image_x_value_label_ = nullptr;
    QLabel* image_y_value_label_ = nullptr;
    QLabel* machine_x_value_label_ = nullptr;
    QLabel* machine_y_value_label_ = nullptr;
    QLabel* angle_value_label_ = nullptr;
    AppConfig app_config_;
    GraspWorkflow workflow_;
    RobotController robot_controller_;
    InputMode input_mode_ = InputMode::Idle;
    cv::Mat current_frame_;
    FrameInferenceResult current_result_;
    QString obb_model_path_;
    QString seg_model_path_;
    QString camera_ip_;
    double camera_exposure_us_ = 0.0;
    double obb_conf_threshold_ = 0.5;
    double obb_nms_threshold_ = 0.3;
    double seg_conf_threshold_ = 0.3;
    double seg_nms_threshold_ = 0.4;
    bool admin_mode_ = false;
    double angle_offset_deg_ = 0.0;
    double center_ray_offset_px_ = 10.0;
    bool show_plc_center_debug_ = false;
    bool show_head_ray_debug_ = true;
    bool postprocess_debug_logging_enabled_ = false;
    bool show_grab_limit_overlay_ = true;
    bool angle_reverse_direction_ = false;
    AngleRangeMode angle_range_mode_ = AngleRangeMode::ZeroTo360;
    AxisMappingMode axis_mapping_mode_ = AxisMappingMode::FrontBackMachineY;
    double front_back_offset_ = 0.0;
    double left_right_offset_ = 0.0;
    QString current_image_path_;
    GrabLimitConfig grab_limits_;
    CoordinateTransformConfig coordinate_transform_config_;
    CoordinateTransformState coordinate_transform_state_;
    bool show_all_detections_ = false;
    PlcRuntimeState plc_runtime_state_;
    bool plc_test_result_dialog_pending_ = false;
    QString last_plc_reject_reason_;
    std::atomic_bool camera_ui_update_pending_{false};
    std::atomic_int camera_ui_seen_count_{0};
    std::atomic_int camera_ui_skipped_count_{0};
    std::atomic_int camera_ui_rendered_count_{0};
    QTimer* plc_poll_timer_ = nullptr;
    bool initial_models_attempted_ = false;
    bool models_loading_ = false;
    bool image_detection_running_ = false;
    bool close_after_model_load_ = false;
    QFutureWatcher<QString>* model_load_watcher_ = nullptr;
    QFutureWatcher<QString>* image_detection_watcher_ = nullptr;
    QFutureWatcher<QString>* plc_poll_watcher_ = nullptr;
    QFutureWatcher<QString>* plc_detection_watcher_ = nullptr;
    QFutureWatcher<QString>* plc_test_watcher_ = nullptr;
    QFutureWatcher<ImageLoadResult>* image_load_watcher_ = nullptr;
    bool top_bar_dragging_ = false;
    QPoint top_bar_drag_offset_;
    QPoint top_bar_press_global_pos_;
    qreal top_bar_press_ratio_ = 0.5;
    bool top_bar_press_active_ = false;
    bool resizing_frame_ = false;
    bool side_panel_expanded_ = true;
    int expanded_sidebar_width_ = 360;
    double responsive_scale_ = 1.0;
    bool syncing_splitter_sizes_ = false;
    Qt::Edges active_resize_edges_;
    QRect resize_start_geometry_;
    QPoint resize_start_global_pos_;
};







