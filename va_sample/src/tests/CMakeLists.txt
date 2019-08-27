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

add_executable(DataPacketTest ${CMAKE_CURRENT_LIST_DIR}/DataPacket_test.cpp "${VA_SOURCES}")
target_link_libraries(DataPacketTest pthread)

add_executable(ConnectorRRTest ${CMAKE_CURRENT_LIST_DIR}/ConnectorRR_test.cpp "${VA_SOURCES}")
target_link_libraries(ConnectorRRTest pthread)

add_executable(ThreadBlockTest ${CMAKE_CURRENT_LIST_DIR}/ThreadBlock_test.cpp "${VA_SOURCES}")
target_link_libraries(ThreadBlockTest pthread)
