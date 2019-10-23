cmake_minimum_required(VERSION 3.4)
project (ULT)

set(MFX_INCLUDE $ENV{MFX_HOME}/include/mfx)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g -O0 -I${MFX_INCLUDE}")
#set(ENABLE_EXPORTS false)
#set(CMAKE_EXE_LINKER_FLAGS "")
set(VA_SOURCES "")
set(VA_INCLUDES "")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
include(${CMAKE_CURRENT_LIST_DIR}/../execution/va_srcs.cmake)
link_directories(/opt/intel/mediasdk/lib/)

add_executable(DataPacketTest ${CMAKE_CURRENT_LIST_DIR}/DataPacket_test.cpp "${VA_SOURCES}")
target_link_libraries(DataPacketTest pthread)

add_executable(ConnectorRRTest ${CMAKE_CURRENT_LIST_DIR}/ConnectorRR_test.cpp "${VA_SOURCES}")
target_link_libraries(ConnectorRRTest pthread)

add_executable(ConnectorDispatchTest ${CMAKE_CURRENT_LIST_DIR}/ConnectorDispatch_test.cpp "${VA_SOURCES}" "${DECODE_SOURCES}")
target_link_libraries(ConnectorDispatchTest pthread)

add_executable(ThreadBlockTest ${CMAKE_CURRENT_LIST_DIR}/ThreadBlock_test.cpp "${VA_SOURCES}")
target_link_libraries(ThreadBlockTest pthread)

add_executable(SingleDummyChannelTest ${CMAKE_CURRENT_LIST_DIR}/SingleDummyChannel.cpp "${VA_SOURCES}")
target_link_libraries(SingleDummyChannelTest pthread)

add_executable(MultiDummyChannelTest ${CMAKE_CURRENT_LIST_DIR}/MultiDummyChannel.cpp "${VA_SOURCES}")
target_link_libraries(MultiDummyChannelTest pthread)

include(${CMAKE_CURRENT_LIST_DIR}/../function/va_srcs.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../common/va_srcs.cmake)

add_executable(DecodeBlockTest ${CMAKE_CURRENT_LIST_DIR}/DecodeThreadBlock_test.cpp "${VA_SOURCES}" "${DECODE_SOURCES}")
target_link_libraries(DecodeBlockTest pthread mfx va va-drm)

set (OpenCV_INCLUDE_DIR $ENV{OPENVINO_DIR}/opencv/include)
set (OpenCV_LIB_DIR $ENV{OPENVINO_DIR}/opencv/lib)
set (OpenCV_LIBS opencv_core opencv_video opencv_videoio opencv_imgproc opencv_photo opencv_highgui opencv_imgcodecs)
set (OPENVINO_IE_INCLUDE $ENV{OPENVINO_DIR}/inference_engine/include $ENV{OPENVINO_DIR}/inference_engine/src/extension)
set (OPENVINO_IE_LIB_DIR $ENV{OPENVINO_DIR}/inference_engine/lib/intel64)
set (OPENVINO_IE_LIBS clDNN64 clDNNPlugin inference_engine cpu_extension)

include_directories(${CMAKE_CURRENT_LIST_DIR}/../../libs)
include_directories( ${OpenCV_INCLUDE_DIR} ${OPENVINO_IE_INCLUDE})
link_directories(${OpenCV_LIB_DIR} ${OPENVINO_IE_LIB_DIR})

add_executable(ObjectDetection ObjectDetection.cpp)
target_link_libraries( ObjectDetection detect)

add_executable(InferenceOV InferenceOV_test.cpp)
target_link_libraries( InferenceOV detect)

add_executable(InferenceBlockTest ${CMAKE_CURRENT_LIST_DIR}/InferenceThreadBlock_test.cpp "${VA_SOURCES}" "${DECODE_SOURCES}" "${INFER_SOURCES}" )
target_link_libraries(InferenceBlockTest pthread mfx va va-drm detect)

add_executable(SingleChannelTest ${CMAKE_CURRENT_LIST_DIR}/SingleChannel_test.cpp "${VA_SOURCES}" "${DECODE_SOURCES}" "${INFER_SOURCES}" "${DISPLAY_SOURCES}" )
target_link_libraries(SingleChannelTest pthread mfx va va-drm detect)

add_executable(CropBlockTest ${CMAKE_CURRENT_LIST_DIR}/CropThreadBlock_test.cpp "${VA_SOURCES}" "${DECODE_SOURCES}" "${INFER_SOURCES}")
target_link_libraries(CropBlockTest pthread mfx va va-drm detect)

add_executable(SamplePipeline ${CMAKE_CURRENT_LIST_DIR}/SamplePipeline.cpp "${VA_SOURCES}" "${DECODE_SOURCES}" "${INFER_SOURCES}")
target_link_libraries(SamplePipeline pthread mfx va va-drm detect)
