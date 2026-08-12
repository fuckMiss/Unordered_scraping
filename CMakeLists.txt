cmake_minimum_required(VERSION 3.12)
project(tankeye-openvino)

if(NOT WIN32)
    message(FATAL_ERROR "This project is maintained for Windows only.")
endif()

option(BUILD_QT_APP "Build the Qt application target" ON)
set(TANKEYE_BUNDLED_HIK_MVS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/vendor/hik_mvs")
if(EXISTS "${TANKEYE_BUNDLED_HIK_MVS_ROOT}/Development/Includes/MvCameraControl.h")
    set(TANKEYE_DEFAULT_HIK_MVS_ROOT "${TANKEYE_BUNDLED_HIK_MVS_ROOT}")
else()
    set(TANKEYE_DEFAULT_HIK_MVS_ROOT "D:/MVS/MVS")
endif()
if(EXISTS "${TANKEYE_BUNDLED_HIK_MVS_ROOT}/Runtime/Win64_x64/MvCameraControl.dll")
    set(TANKEYE_DEFAULT_HIK_MVS_RUNTIME_DIR "${TANKEYE_BUNDLED_HIK_MVS_ROOT}/Runtime/Win64_x64")
else()
    set(TANKEYE_DEFAULT_HIK_MVS_RUNTIME_DIR "C:/Program Files (x86)/Common Files/MVS/Runtime/Win64_x64")
endif()
set(HIK_MVS_ROOT "${TANKEYE_DEFAULT_HIK_MVS_ROOT}" CACHE PATH "Hikrobot MVS SDK install root")
set(HIK_MVS_RUNTIME_DIR "${TANKEYE_DEFAULT_HIK_MVS_RUNTIME_DIR}" CACHE PATH "Hikrobot MVS Win64 runtime directory")
set(HIK_MVS_INCLUDE_DIR "${HIK_MVS_ROOT}/Development/Includes" CACHE PATH "Hikrobot MVS include directory")
set(HIK_MVS_LIBRARY "${HIK_MVS_ROOT}/Development/Libraries/win64/MvCameraControl.lib" CACHE FILEPATH "Hikrobot MVS Win64 import library")

set(OPENVINO_SEARCH_ROOTS
    $ENV{OpenVINO_DIR}
    $ENV{openvino_DIR}
    $ENV{OPENVINO_ROOT}
    "$ENV{CONDA_PREFIX}/Lib/site-packages/openvino"
    "$ENV{CONDA_PREFIX}/Library"
    "$ENV{CONDA_PREFIX}"
    "C:/Program Files (x86)/Intel/openvino"
)

find_package(OpenVINO QUIET COMPONENTS Runtime)
if(NOT OpenVINO_FOUND)
    find_package(openvino QUIET COMPONENTS Runtime
        HINTS ${OPENVINO_SEARCH_ROOTS}
        PATH_SUFFIXES
            runtime/cmake
            cmake
            share/cmake/openvino
            tools/cmake
    )
endif()
if(NOT OpenVINO_FOUND AND NOT openvino_FOUND)
    message(FATAL_ERROR
        "OpenVINO C++ runtime CMake package was not found.\n"
        "Find the directory containing openvinoConfig.cmake or OpenVINOConfig.cmake, then rerun CMake with one of:\n"
        "  set openvino_DIR=PATH_TO_THE_CONFIG_DIRECTORY\n"
        "  set OpenVINO_DIR=PATH_TO_THE_CONFIG_DIRECTORY\n"
        "For pip/conda OpenVINO, try searching with:\n"
        "  dir /s /b %CONDA_PREFIX%\\*openvino*Config.cmake\n"
        "If no config file exists, install the OpenVINO C++ Runtime/Dev package, not only the Python wheel."
    )
endif()
find_package(OpenCV REQUIRED COMPONENTS
    highgui
    ml
    objdetect
    photo
    stitching
    video
    calib3d
    features2d
    dnn
    flann
    videoio
    imgcodecs
    imgproc
    core
)

if(BUILD_QT_APP)
    set(TANKEYE_QT_ROOT "D:/Qt/5.15.2/msvc2019_64" CACHE PATH "Qt 5.15.2 msvc2019_64 root directory")
    if(NOT Qt5_DIR AND EXISTS "${TANKEYE_QT_ROOT}/lib/cmake/Qt5")
        set(Qt5_DIR "${TANKEYE_QT_ROOT}/lib/cmake/Qt5" CACHE PATH "Qt5 config directory")
    endif()
    if(NOT Qt5Core_DIR AND EXISTS "${TANKEYE_QT_ROOT}/lib/cmake/Qt5Core")
        set(Qt5Core_DIR "${TANKEYE_QT_ROOT}/lib/cmake/Qt5Core" CACHE PATH "Qt5Core config directory")
    endif()
    if(NOT Qt5Gui_DIR AND EXISTS "${TANKEYE_QT_ROOT}/lib/cmake/Qt5Gui")
        set(Qt5Gui_DIR "${TANKEYE_QT_ROOT}/lib/cmake/Qt5Gui" CACHE PATH "Qt5Gui config directory")
    endif()
    if(NOT Qt5Widgets_DIR AND EXISTS "${TANKEYE_QT_ROOT}/lib/cmake/Qt5Widgets")
        set(Qt5Widgets_DIR "${TANKEYE_QT_ROOT}/lib/cmake/Qt5Widgets" CACHE PATH "Qt5Widgets config directory")
    endif()
    if(NOT Qt5Network_DIR AND EXISTS "${TANKEYE_QT_ROOT}/lib/cmake/Qt5Network")
        set(Qt5Network_DIR "${TANKEYE_QT_ROOT}/lib/cmake/Qt5Network" CACHE PATH "Qt5Network config directory")
    endif()
    find_package(Qt5 REQUIRED COMPONENTS Widgets Network Concurrent)
endif()

set(CMAKE_AUTOMOC ${BUILD_QT_APP})
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8)
endif()

if(OpenVINO_FOUND)
    message(STATUS "OpenVINO version: ${OpenVINO_VERSION}")
else()
    message(STATUS "OpenVINO version: ${openvino_VERSION}")
endif()
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")
message(STATUS "OpenCV libraries: ${OpenCV_LIBS}")

set(PROJECT_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/core/common
    ${CMAKE_CURRENT_SOURCE_DIR}/core/inference
    ${OpenCV_INCLUDE_DIRS}
)

set(COMMON_SOURCES
    core/common/deploy_utils.cpp
    core/inference/openvino_utils.cpp
)
set(COMMON_HEADERS
    core/common/deploy_utils.h
    core/common/macros.h
    core/common/model_utils.h
    core/inference/openvino_utils.h
    core/common/common.h
)

set(QT_WORKFLOW_BUSINESS_SOURCES
    app/qt/workflow/app_config_service.cpp
    app/qt/workflow/app_startup_manager.cpp
    app/qt/workflow/engineering_settings_service.cpp
    app/qt/workflow/project_profile_store.cpp
    app/qt/workflow/frame_processing_service.cpp
    app/qt/workflow/grab_limit_evaluator.cpp
    app/qt/workflow/plc_trigger_coordinator.cpp
    app/qt/workflow/plc_result_contract.cpp
)
set(QT_WORKFLOW_BUSINESS_HEADERS
    app/qt/workflow/app_config_service.h
    app/qt/workflow/app_startup_manager.h
    app/qt/workflow/engineering_settings_service.h
    app/qt/workflow/project_profile_store.h
    app/qt/workflow/frame_processing_service.h
    app/qt/workflow/grab_limit_evaluator.h
    app/qt/workflow/plc_trigger_coordinator.h
    app/qt/workflow/plc_result_contract.h
)

function(configure_yolov11_target target_name)
    target_compile_definitions(${target_name} PRIVATE API_EXPORTS)
    target_include_directories(${target_name} PUBLIC ${PROJECT_INCLUDE_DIRS})
    target_link_libraries(${target_name}
        ${OpenCV_LIBS}
        openvino::runtime
    )
endfunction()

add_library(${PROJECT_NAME}_deploy_common STATIC
    ${COMMON_SOURCES}
    ${COMMON_HEADERS}
)
configure_yolov11_target(${PROJECT_NAME}_deploy_common)

add_library(${PROJECT_NAME}_deploy_obb STATIC
    core/inference/YOLOv11_OBB.cpp
    core/inference/YOLOv11_OBB.h
)
configure_yolov11_target(${PROJECT_NAME}_deploy_obb)
target_link_libraries(${PROJECT_NAME}_deploy_obb ${PROJECT_NAME}_deploy_common)

add_library(${PROJECT_NAME}_deploy_seg STATIC
    core/inference/YOLOv11_SEG.cpp
    core/inference/YOLOv11_SEG.h
)
configure_yolov11_target(${PROJECT_NAME}_deploy_seg)
target_link_libraries(${PROJECT_NAME}_deploy_seg ${PROJECT_NAME}_deploy_common)

add_executable(${PROJECT_NAME}_obb app/cli/main_obb.cpp)
target_link_libraries(${PROJECT_NAME}_obb ${PROJECT_NAME}_deploy_obb)

add_executable(${PROJECT_NAME}_seg app/cli/main_seg.cpp)
target_link_libraries(${PROJECT_NAME}_seg ${PROJECT_NAME}_deploy_seg)

add_executable(${PROJECT_NAME}_device_probe app/cli/main_device_probe.cpp)
target_link_libraries(${PROJECT_NAME}_device_probe openvino::runtime)

enable_testing()

function(add_tankeye_test test_name)
    cmake_parse_arguments(TEST "" "" "SOURCES;INCLUDE_DIRS;LIBS" ${ARGN})
    set(target_name ${PROJECT_NAME}_${test_name})
    add_executable(${target_name} ${TEST_SOURCES})
    target_include_directories(${target_name} PRIVATE ${TEST_INCLUDE_DIRS})
    target_link_libraries(${target_name} ${TEST_LIBS})
    add_test(NAME ${test_name} COMMAND ${target_name})
endfunction()

add_tankeye_test(frame_postprocess_smoke
    SOURCES
        tests/frame_postprocess_smoke.cpp
        app/qt/workflow/frame_postprocess.cpp
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/app/qt/workflow
        ${CMAKE_CURRENT_SOURCE_DIR}/core/common
        ${CMAKE_CURRENT_SOURCE_DIR}/core/inference
        ${OpenCV_INCLUDE_DIRS}
    LIBS
        ${OpenCV_LIBS}
        openvino::runtime
)

add_tankeye_test(plc_result_contract_test
    SOURCES
        tests/plc_result_contract_test.cpp
        app/qt/workflow/plc_result_contract.cpp
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/app/qt/workflow
        ${OpenCV_INCLUDE_DIRS}
    LIBS
        ${OpenCV_LIBS}
)

if(BUILD_QT_APP)
    add_tankeye_test(app_config_service_test
        SOURCES
            tests/app_config_service_test.cpp
            app/qt/workflow/app_config_service.cpp
            app/qt/workflow/plc_result_contract.cpp
        INCLUDE_DIRS
            ${CMAKE_CURRENT_SOURCE_DIR}/app/qt/workflow
            ${OpenCV_INCLUDE_DIRS}
        LIBS
            Qt5::Core
            ${OpenCV_LIBS}
    )

    add_tankeye_test(robot_controller_register_map_test
        SOURCES
            tests/robot_controller_register_map_test.cpp
            app/qt/hardware/robot_controller.cpp
            app/qt/workflow/app_config_service.cpp
            app/qt/workflow/plc_result_contract.cpp
        INCLUDE_DIRS
            ${CMAKE_CURRENT_SOURCE_DIR}/app/qt/hardware
            ${CMAKE_CURRENT_SOURCE_DIR}/app/qt/workflow
            ${OpenCV_INCLUDE_DIRS}
        LIBS
            Qt5::Core
            Qt5::Network
            ${OpenCV_LIBS}
    )

    add_tankeye_test(coordinate_transform_test
        SOURCES
            tests/coordinate_transform_test.cpp
            app/qt/workflow/coordinate_transform.cpp
        INCLUDE_DIRS
            ${CMAKE_CURRENT_SOURCE_DIR}/app/qt/workflow
            ${OpenCV_INCLUDE_DIRS}
        LIBS
            Qt5::Core
            ${OpenCV_LIBS}
    )

    add_tankeye_test(grab_limit_evaluator_test
        SOURCES
            tests/grab_limit_evaluator_test.cpp
            app/qt/workflow/coordinate_transform.cpp
            app/qt/workflow/grab_limit_evaluator.cpp
            app/qt/workflow/plc_result_contract.cpp
        INCLUDE_DIRS
            ${CMAKE_CURRENT_SOURCE_DIR}/app/qt/workflow
            ${OpenCV_INCLUDE_DIRS}
        LIBS
            Qt5::Core
            ${OpenCV_LIBS}
    )

    add_tankeye_test(engineering_settings_service_test
        SOURCES
            tests/engineering_settings_service_test.cpp
            app/qt/workflow/app_startup_manager.cpp
            app/qt/workflow/app_config_service.cpp
            app/qt/workflow/engineering_settings_service.cpp
            app/qt/workflow/calibration_profile_store.cpp
            app/qt/workflow/coordinate_transform.cpp
            app/qt/workflow/grab_limit_evaluator.cpp
            app/qt/workflow/plc_result_contract.cpp
        INCLUDE_DIRS
            ${CMAKE_CURRENT_SOURCE_DIR}/app/qt/workflow
            ${OpenCV_INCLUDE_DIRS}
        LIBS
            Qt5::Core
            ${OpenCV_LIBS}
    )

    add_tankeye_test(calibration_profile_store_test
        SOURCES
            tests/calibration_profile_store_test.cpp
            app/qt/workflow/calibration_profile_store.cpp
        INCLUDE_DIRS
            ${CMAKE_CURRENT_SOURCE_DIR}/app/qt/workflow
            ${OpenCV_INCLUDE_DIRS}
        LIBS
            Qt5::Core
            ${OpenCV_LIBS}
    )

    add_tankeye_test(project_profile_store_test
        SOURCES
            tests/project_profile_store_test.cpp
            app/qt/workflow/project_profile_store.cpp
            app/qt/workflow/engineering_settings_service.cpp
            app/qt/workflow/calibration_profile_store.cpp
            app/qt/workflow/coordinate_transform.cpp
            app/qt/workflow/grab_limit_evaluator.cpp
            app/qt/workflow/plc_result_contract.cpp
        INCLUDE_DIRS
            ${CMAKE_CURRENT_SOURCE_DIR}/app/qt/workflow
            ${CMAKE_CURRENT_SOURCE_DIR}/core/common
            ${CMAKE_CURRENT_SOURCE_DIR}/core/inference
            ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
            ${OpenCV_INCLUDE_DIRS}
        LIBS
            Qt5::Core
            Qt5::Gui
            ${OpenCV_LIBS}
    )

    add_tankeye_test(runtime_status_presenter_test
        SOURCES
            tests/runtime_status_presenter_test.cpp
            app/qt/ui/runtime_status_presenter.cpp
        INCLUDE_DIRS
            ${CMAKE_CURRENT_SOURCE_DIR}/app/qt/ui
            ${CMAKE_CURRENT_SOURCE_DIR}/app/qt/workflow
            ${OpenCV_INCLUDE_DIRS}
        LIBS
            Qt5::Widgets
            ${OpenCV_LIBS}
    )

    add_tankeye_test(engineering_settings_spinbox_wheel_test
        SOURCES
            tests/engineering_settings_spinbox_wheel_test.cpp
            app/qt/ui/engineering_settings_dialog_helpers.cpp
            app/qt/workflow/calibration_profile_store.cpp
            app/qt/workflow/coordinate_transform.cpp
        INCLUDE_DIRS
            ${CMAKE_CURRENT_SOURCE_DIR}/app/qt/ui
            ${CMAKE_CURRENT_SOURCE_DIR}/app/qt/workflow
            ${OpenCV_INCLUDE_DIRS}
        LIBS
            Qt5::Widgets
            ${OpenCV_LIBS}
    )

    add_tankeye_test(model_folder_detector_test
        SOURCES
            tests/model_folder_detector_test.cpp
            app/qt/ui/engineering_settings_dialog_helpers.cpp
            app/qt/workflow/calibration_profile_store.cpp
            app/qt/workflow/coordinate_transform.cpp
        INCLUDE_DIRS
            ${CMAKE_CURRENT_SOURCE_DIR}/app/qt/ui
            ${CMAKE_CURRENT_SOURCE_DIR}/app/qt/workflow
            ${OpenCV_INCLUDE_DIRS}
        LIBS
            Qt5::Widgets
            ${OpenCV_LIBS}
    )

    add_tankeye_test(frame_overlay_test
        SOURCES
            tests/frame_overlay_test.cpp
            app/qt/ui/frame_overlay.cpp
        INCLUDE_DIRS
            ${CMAKE_CURRENT_SOURCE_DIR}/app/qt/ui
            ${CMAKE_CURRENT_SOURCE_DIR}/app/qt/workflow
            ${CMAKE_CURRENT_SOURCE_DIR}/core/common
            ${OpenCV_INCLUDE_DIRS}
        LIBS
            ${OpenCV_LIBS}
    )

    add_tankeye_test(admin_auth_helpers_test
        SOURCES
            tests/admin_auth_helpers_test.cpp
            app/qt/ui/admin_auth_helpers.cpp
        INCLUDE_DIRS
            ${CMAKE_CURRENT_SOURCE_DIR}/app/qt/ui
        LIBS
            Qt5::Widgets
            Qt5::Network
    )

    add_tankeye_test(plc_runtime_state_test
        SOURCES
            tests/plc_runtime_state_test.cpp
            app/qt/ui/plc_runtime_state.cpp
            app/qt/ui/runtime_status_presenter.cpp
        INCLUDE_DIRS
            ${CMAKE_CURRENT_SOURCE_DIR}/app/qt/ui
            ${CMAKE_CURRENT_SOURCE_DIR}/app/qt/workflow
            ${OpenCV_INCLUDE_DIRS}
        LIBS
            Qt5::Widgets
            ${OpenCV_LIBS}
    )
endif()

if(BUILD_QT_APP)
    add_executable(${PROJECT_NAME}_qt_app
        app/qt/main.cpp
        app/qt/app_icon.rc
        app/qt/app_resources.qrc
        app/qt/hardware/hik_camera.cpp
        app/qt/hardware/hik_camera.h
        app/qt/hardware/robot_controller.cpp
        app/qt/hardware/robot_controller.h
        app/qt/ui/grasp_main_window.cpp
        app/qt/ui/grasp_main_window_settings.cpp
        app/qt/ui/grasp_main_window.h
        app/qt/ui/admin_auth_dialogs.cpp
        app/qt/ui/admin_auth_dialogs.h
        app/qt/ui/admin_auth_helpers.cpp
        app/qt/ui/admin_auth_helpers.h
        app/qt/ui/engineering_settings_dialog.cpp
        app/qt/ui/engineering_settings_dialog_controller.h
        app/qt/ui/runtime_log_dialog.cpp
        app/qt/ui/runtime_log_dialog.h
        app/qt/ui/runtime_status_presenter.cpp
        app/qt/ui/runtime_status_presenter.h
        app/qt/ui/plc_runtime_state.cpp
        app/qt/ui/plc_runtime_state.h
        app/qt/ui/engineering_settings_dialog_helpers.cpp
        app/qt/ui/engineering_settings_dialog_helpers.h
        app/qt/ui/frame_overlay.cpp
        app/qt/ui/frame_overlay.h
        app/qt/workflow/calibration_profile_store.cpp
        app/qt/workflow/calibration_profile_store.h
        app/qt/workflow/coordinate_transform.cpp
        app/qt/workflow/coordinate_transform.h
        app/qt/workflow/grasp_workflow.cpp
        app/qt/workflow/grasp_workflow.h
        app/qt/workflow/frame_postprocess.cpp
        app/qt/workflow/frame_postprocess.h
        app/qt/workflow/frame_result.h
        ${QT_WORKFLOW_BUSINESS_SOURCES}
        ${QT_WORKFLOW_BUSINESS_HEADERS}
    )
    target_include_directories(${PROJECT_NAME}_qt_app PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/app/qt
        ${CMAKE_CURRENT_SOURCE_DIR}/app/qt/hardware
        ${CMAKE_CURRENT_SOURCE_DIR}/app/qt/ui
        ${CMAKE_CURRENT_SOURCE_DIR}/app/qt/workflow
        ${CMAKE_CURRENT_SOURCE_DIR}/core/common
        ${CMAKE_CURRENT_SOURCE_DIR}/core/inference
        ${HIK_MVS_INCLUDE_DIR}
        ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
    )
    set_target_properties(${PROJECT_NAME}_qt_app PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_libraries(${PROJECT_NAME}_qt_app
        Qt5::Widgets
        Qt5::Network
        Qt5::Concurrent
        ${PROJECT_NAME}_deploy_common
        ${PROJECT_NAME}_deploy_obb
        ${PROJECT_NAME}_deploy_seg
        ${HIK_MVS_LIBRARY}
    )
    if(EXISTS "${HIK_MVS_RUNTIME_DIR}/MvCameraControl.dll")
        add_custom_command(TARGET ${PROJECT_NAME}_qt_app POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${HIK_MVS_RUNTIME_DIR}"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}_qt_app>"
        )
    endif()
    add_custom_command(TARGET ${PROJECT_NAME}_qt_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${PROJECT_NAME}_qt_app>/config"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/config/tankeye.json"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}_qt_app>/config/tankeye.json"
    )
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config/project_profiles")
        add_custom_command(TARGET ${PROJECT_NAME}_qt_app POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/config/project_profiles"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}_qt_app>/config/project_profiles"
            VERBATIM
        )
    endif()

    add_executable(tankeye-admin-auth-code
        app/qt/tools/admin_auth_code_tool.cpp
        app/qt/ui/admin_auth_helpers.cpp
        app/qt/ui/admin_auth_helpers.h
    )
    target_include_directories(tankeye-admin-auth-code PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/app/qt/ui
    )
    target_link_libraries(tankeye-admin-auth-code
        Qt5::Widgets
        Qt5::Network
    )
endif()
