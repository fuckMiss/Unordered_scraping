cmake_minimum_required(VERSION 3.12)
project(tankeye-openvino)

if(NOT WIN32)
    message(FATAL_ERROR "This project is maintained for Windows only.")
endif()

option(BUILD_QT_APP "Build the Qt application target" ON)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
)

set(COMMON_SOURCES
    src/deploy_utils.cpp
    src/openvino_utils.cpp
)
set(COMMON_HEADERS
    src/deploy_utils.h
    src/macros.h
    src/model_utils.h
    src/openvino_utils.h
    src/common.h
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
    src/YOLOv11_OBB.cpp
    src/YOLOv11_OBB.h
)
configure_yolov11_target(${PROJECT_NAME}_deploy_obb)
target_link_libraries(${PROJECT_NAME}_deploy_obb ${PROJECT_NAME}_deploy_common)

add_library(${PROJECT_NAME}_deploy_seg STATIC
    src/YOLOv11_SEG.cpp
    src/YOLOv11_SEG.h
)
configure_yolov11_target(${PROJECT_NAME}_deploy_seg)
target_link_libraries(${PROJECT_NAME}_deploy_seg ${PROJECT_NAME}_deploy_common)

add_executable(${PROJECT_NAME}_obb main_obb.cpp)
target_link_libraries(${PROJECT_NAME}_obb ${PROJECT_NAME}_deploy_obb)

add_executable(${PROJECT_NAME}_seg main_seg.cpp)
target_link_libraries(${PROJECT_NAME}_seg ${PROJECT_NAME}_deploy_seg)

if(BUILD_QT_APP)
    add_executable(${PROJECT_NAME}_qt_app
        qt/main.cpp
        qt/app_icon.rc
        qt/app_resources.qrc
        qt/grasp_main_window.cpp
        qt/grasp_main_window.h
        qt/grasp_workflow.cpp
        qt/grasp_workflow.h
        qt/robot_controller.cpp
        qt/robot_controller.h
        qt/frame_overlay.cpp
        qt/frame_overlay.h
        qt/frame_postprocess.cpp
        qt/frame_postprocess.h
        qt/frame_result.h
    )
    target_include_directories(${PROJECT_NAME}_qt_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/qt)
    set_target_properties(${PROJECT_NAME}_qt_app PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_libraries(${PROJECT_NAME}_qt_app
        Qt5::Widgets
        Qt5::Network
        Qt5::Concurrent
        ${PROJECT_NAME}_deploy_common
        ${PROJECT_NAME}_deploy_obb
        ${PROJECT_NAME}_deploy_seg
    )
endif()








