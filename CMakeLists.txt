cmake_minimum_required(VERSION 3.12)
project(yolov11-tensorrt)

find_package(CUDA REQUIRED)
# Force the system Qt installation so the build does not accidentally mix Anaconda Qt
# with the system libstdc++/libffi toolchain at link time.
set(Qt5_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5" CACHE PATH "Qt5 config directory" FORCE)
set(Qt5Core_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5Core" CACHE PATH "Qt5Core config directory" FORCE)
set(Qt5Gui_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5Gui" CACHE PATH "Qt5Gui config directory" FORCE)
set(Qt5Widgets_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5Widgets" CACHE PATH "Qt5Widgets config directory" FORCE)
find_package(Qt5 REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find and include OpenCV
set(OpenCV_DIR "/home/cll/下载/opencv/build" CACHE PATH "OpenCV config directory")
set(OPENCV_ROOT "/home/cll/下载/opencv")
set(OPENCV_LIB_DIR "${OpenCV_DIR}/lib")

# Set TensorRT path if not set in environment variables
set(TENSORRT_DIR "/opt/TensorRT-8.6.1.6")
set(TENSORRT_INCLUDE_DIR "${TENSORRT_DIR}/include")
set(TENSORRT_LIB_DIR "${TENSORRT_DIR}/lib")

# Include directories
file(GLOB OPENCV_MODULE_INCLUDE_DIRS "${OPENCV_ROOT}/modules/*/include")
set(PROJECT_INCLUDE_DIRS
    ${CUDA_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${TENSORRT_INCLUDE_DIR}
    ${OpenCV_DIR}
    ${OPENCV_ROOT}/include
    ${OPENCV_MODULE_INCLUDE_DIRS}
)

set(OPENCV_MODULES
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
set(OpenCV_LIBS)
foreach(module IN LISTS OPENCV_MODULES)
    list(APPEND OpenCV_LIBS "${OPENCV_LIB_DIR}/libopencv_${module}.so")
endforeach()

set(TENSORRT_LIBS nvinfer nvinfer_plugin nvparsers nvonnxparser)

# Add CUDA source files
set(CUDA_SOURCES src/preprocess.cu)
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

add_library(${PROJECT_NAME}_deploy_det STATIC
    src/YOLOv11.cpp
    src/YOLOv11.h
)
configure_yolov11_target(${PROJECT_NAME}_deploy_det)
target_link_libraries(${PROJECT_NAME}_deploy_det ${PROJECT_NAME}_deploy_common)

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

# ========== Standard YOLOv11 detection executable ==========
add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}_deploy_det)

# ========== YOLOv11 OBB detection executable ==========
add_executable(${PROJECT_NAME}_obb main_obb.cpp)
target_link_libraries(${PROJECT_NAME}_obb ${PROJECT_NAME}_deploy_obb)

# ========== YOLOv11 SEG detection executable ==========
add_executable(${PROJECT_NAME}_seg main_seg.cpp)
target_link_libraries(${PROJECT_NAME}_seg ${PROJECT_NAME}_deploy_seg)

# ========== Qt application layer executable ==========
add_executable(${PROJECT_NAME}_qt_app
    qt/main.cpp
    qt/grasp_main_window.cpp
    qt/grasp_main_window.h
    qt/grasp_workflow.cpp
    qt/grasp_workflow.h
    qt/frame_overlay.cpp
    qt/frame_overlay.h
    qt/frame_postprocess.cpp
    qt/frame_postprocess.h
    qt/frame_result.h
)
target_include_directories(${PROJECT_NAME}_qt_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/qt)
target_link_libraries(${PROJECT_NAME}_qt_app
    Qt5::Widgets
    ${PROJECT_NAME}_deploy_common
    ${PROJECT_NAME}_deploy_obb
    ${PROJECT_NAME}_deploy_seg
)

# /opt/TensorRT-8.6.1.6/bin/trtexec --onnx=./weights/yolo11s-seg.onnx --saveEngine=./weights/yolo11s-seg.engine --fp16

