cmake_minimum_required(VERSION 3.12)
project(yolov11-tensorrt)

option(BUILD_QT_APP "Build the Qt application target" ON)

set(CUDA_USE_STATIC_CUDA_RUNTIME OFF)
find_package(CUDA REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS
    gapi
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
    if(UNIX AND NOT APPLE)
        # Force the system Qt installation so the build does not accidentally mix Anaconda Qt
        # with the system libstdc++/libffi toolchain at link time.
        set(Qt5_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5" CACHE PATH "Qt5 config directory" FORCE)
        set(Qt5Core_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5Core" CACHE PATH "Qt5Core config directory" FORCE)
        set(Qt5Gui_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5Gui" CACHE PATH "Qt5Gui config directory" FORCE)
        set(Qt5Widgets_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5Widgets" CACHE PATH "Qt5Widgets config directory" FORCE)
    endif()
    find_package(Qt5 REQUIRED COMPONENTS Widgets Network)
endif()

set(CMAKE_AUTOMOC ${BUILD_QT_APP})

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# Find TensorRT from apt, tar installs, or explicit -DTENSORRT_ROOT=/path.
set(TENSORRT_ROOT "" CACHE PATH "TensorRT root directory")
set(TENSORRT_SEARCH_ROOTS
    ${TENSORRT_ROOT}
    $ENV{TENSORRT_ROOT}
    $ENV{TENSORRT_DIR}
    /usr
    /usr/local
    /opt/TensorRT
    /opt/TensorRT-10.6.0.26
    /opt/TensorRT-10.6.0
    /opt/TensorRT-8.6.1.6
)
if(WIN32)
    list(APPEND TENSORRT_SEARCH_ROOTS
        "D:/Tensorrt/TensorRT-8.6.1.6"
        "C:/TensorRT-8.6.1.6"
        "C:/TensorRT"
    )
endif()
find_path(TENSORRT_INCLUDE_DIR
    NAMES NvInfer.h
    HINTS ${TENSORRT_SEARCH_ROOTS}
    PATH_SUFFIXES include include/x86_64-linux-gnu
    REQUIRED
)
find_library(TENSORRT_NVINFER_LIBRARY
    NAMES nvinfer
    HINTS ${TENSORRT_SEARCH_ROOTS}
    PATH_SUFFIXES lib lib64 lib/x86_64-linux-gnu
    REQUIRED
)
find_library(TENSORRT_NVINFER_PLUGIN_LIBRARY
    NAMES nvinfer_plugin
    HINTS ${TENSORRT_SEARCH_ROOTS}
    PATH_SUFFIXES lib lib64 lib/x86_64-linux-gnu
    REQUIRED
)
find_library(TENSORRT_NVONNXPARSER_LIBRARY
    NAMES nvonnxparser
    HINTS ${TENSORRT_SEARCH_ROOTS}
    PATH_SUFFIXES lib lib64 lib/x86_64-linux-gnu
    REQUIRED
)
find_library(TENSORRT_NVPARSERS_LIBRARY
    NAMES nvparsers
    HINTS ${TENSORRT_SEARCH_ROOTS}
    PATH_SUFFIXES lib lib64 lib/x86_64-linux-gnu
)
get_filename_component(TENSORRT_LIB_DIR "${TENSORRT_NVINFER_LIBRARY}" DIRECTORY)
set(TENSORRT_LIBS
    ${TENSORRT_NVINFER_LIBRARY}
    ${TENSORRT_NVINFER_PLUGIN_LIBRARY}
    ${TENSORRT_NVONNXPARSER_LIBRARY}
)
if(TENSORRT_NVPARSERS_LIBRARY)
    list(APPEND TENSORRT_LIBS ${TENSORRT_NVPARSERS_LIBRARY})
endif()
message(STATUS "TensorRT include dir: ${TENSORRT_INCLUDE_DIR}")
message(STATUS "TensorRT library dir: ${TENSORRT_LIB_DIR}")
message(STATUS "TensorRT libraries: ${TENSORRT_LIBS}")
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")
message(STATUS "OpenCV libraries: ${OpenCV_LIBS}")

# Include directories
set(PROJECT_INCLUDE_DIRS
    ${CUDA_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${TENSORRT_INCLUDE_DIR}
    ${OpenCV_INCLUDE_DIRS}
)

# Add CUDA source files
set(CUDA_SOURCES src/preprocess.cu)
if(WIN32)
    list(APPEND CUDA_NVCC_FLAGS --allow-unsupported-compiler -D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH)
endif()
set(COMMON_SOURCES
    src/deploy_utils.cpp
    src/trt_utils.cpp
)
set(COMMON_HEADERS
    src/deploy_utils.h
    src/macros.h
    src/logging.h
    src/model_utils.h
    src/runtime_utils.h
    src/trt_utils.h
    src/cuda_utils.h
    src/preprocess.h
    src/common.h
)

function(configure_yolov11_target target_name)
    target_compile_definitions(${target_name} PRIVATE API_EXPORTS)
    target_include_directories(${target_name} PUBLIC ${PROJECT_INCLUDE_DIRS})
    target_link_directories(${target_name} PUBLIC ${TENSORRT_LIB_DIR})
    target_link_libraries(${target_name}
        ${OpenCV_LIBS}
        ${CUDA_LIBRARIES}
        ${TENSORRT_LIBS}
    )
endfunction()

cuda_add_library(${PROJECT_NAME}_deploy_common STATIC
    ${COMMON_SOURCES}
    ${COMMON_HEADERS}
    ${CUDA_SOURCES}
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

# ========== YOLOv11 OBB detection executable ==========
add_executable(${PROJECT_NAME}_obb main_obb.cpp)
target_link_libraries(${PROJECT_NAME}_obb ${PROJECT_NAME}_deploy_obb)

# ========== YOLOv11 SEG detection executable ==========
add_executable(${PROJECT_NAME}_seg main_seg.cpp)
target_link_libraries(${PROJECT_NAME}_seg ${PROJECT_NAME}_deploy_seg)

if(BUILD_QT_APP)
    # ========== Qt application layer executable ==========
    add_executable(${PROJECT_NAME}_qt_app
        qt/main.cpp
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
    if(WIN32)
        set_target_properties(${PROJECT_NAME}_qt_app PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()
    target_link_libraries(${PROJECT_NAME}_qt_app
        Qt5::Widgets
        Qt5::Network
        ${PROJECT_NAME}_deploy_common
        ${PROJECT_NAME}_deploy_obb
        ${PROJECT_NAME}_deploy_seg
    )
endif()

# /opt/TensorRT-8.6.1.6/bin/trtexec --onnx=./weights/yolo11s-seg.onnx --saveEngine=./weights/yolo11s-seg.engine --fp16
