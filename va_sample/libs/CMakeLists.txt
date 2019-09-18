cmake_minimum_required(VERSION 2.8)

project( detect )

set (CMAKE_CXX_STANDARD 11)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")

set (OpenCV_INCLUDE_DIR $ENV{OPENVINO_DIR}/opencv/include)
set (OpenCV_LIB_DIR $ENV{OPENVINO_DIR}/opencv/lib)
set (OpenCV_LIBS opencv_core opencv_video opencv_videoio opencv_imgproc opencv_photo opencv_highgui opencv_imgcodecs)
set (OPENVINO_IE_INCLUDE $ENV{OPENVINO_DIR}/inference_engine/include $ENV{OPENVINO_DIR}/inference_engine/src/extension)
set (OPENVINO_IE_LIB_DIR $ENV{OPENVINO_DIR}/inference_engine/lib/intel64)
set (OPENVINO_IE_LIBS clDNN64 clDNNPlugin inference_engine cpu_extension)

include_directories( ${OpenCV_INCLUDE_DIR} ${OPENVINO_IE_INCLUDE})
link_directories(${OpenCV_LIB_DIR} ${OPENVINO_IE_LIB_DIR})


add_library(detect SHARED detector.cpp detector.h)

set_target_properties(detect PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries( detect ${OpenCV_LIBS} ${OPENVINO_IE_LIBS})

