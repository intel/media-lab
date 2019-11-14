#
# Copyright (c) 2019, Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
#

cmake_minimum_required(VERSION 2.8)

project( detect )

set (CMAKE_CXX_STANDARD 11)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")

if(DEFINED ENV{OPENVINO_DIR})
    set (OpenCV_INCLUDE_DIR $ENV{OPENVINO_DIR}/opencv/include)
    set (OpenCV_LIB_DIR $ENV{OPENVINO_DIR}/opencv/lib)
    set (OPENVINO_IE_INCLUDE $ENV{OPENVINO_DIR}/inference_engine/include $ENV{OPENVINO_DIR}/inference_engine/src/extension)
    set (OPENVINO_IE_LIB_DIR $ENV{OPENVINO_DIR}/inference_engine/lib/intel64)
elseif(DEFINED ENV{DLDT_DIR})
    set (OpenCV_INCLUDE_DIR $ENV{DLDT_DIR}/inference-engine/temp/opencv_4.1.2_ubuntu18/include)
    set (OpenCV_LIB_DIR $ENV{DLDT_DIR}/inference-engine/temp/opencv_4.1.2_ubuntu18/lib $ENV{DLDT_DIR}/inference-engine/temp/tbb/lib)
    set (OPENVINO_IE_INCLUDE $ENV{DLDT_DIR}/inference-engine/include $ENV{DLDT_DIR}/inference-engine/src/extension)
    set (OPENVINO_IE_LIB_DIR $ENV{DLDT_DIR}/inference-engine/bin/intel64/Release/lib)
else()
    message( SEND_ERROR "OPENVINO_DIR or DLDT_DIR not set" )
endif()
set (OpenCV_LIBS opencv_core opencv_video opencv_videoio opencv_imgproc opencv_photo opencv_highgui opencv_imgcodecs)
set (OPENVINO_IE_LIBS clDNN64 clDNNPlugin inference_engine cpu_extension)

include_directories( ${OpenCV_INCLUDE_DIR} ${OPENVINO_IE_INCLUDE} ${CMAKE_CURRENT_LIST_DIR}/../src/execution $ENV{MFX_HOME}/include/mfx)
link_directories(${OpenCV_LIB_DIR} ${OPENVINO_IE_LIB_DIR})


add_library(detect SHARED detector.cpp detector.h Inference.h Inference.cpp InferenceOV.cpp InferenceMobileSSD.cpp InferenceResnet50.cpp ${CMAKE_CURRENT_LIST_DIR}/../src/execution/DataPacket.cpp)

set_target_properties(detect PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries( detect ${OpenCV_LIBS} ${OPENVINO_IE_LIBS})

