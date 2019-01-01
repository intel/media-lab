cmake_minimum_required(VERSION 3.4)
project (VAIT)


list (APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

message(STATUS "Looking for inference engine configuration file at: ${CMAKE_PREFIX_PATH}")
find_package(InferenceEngine 1.0)


include(feature_defs ON)

####################################
## to use C++11
set (CMAKE_CXX_STANDARD 11)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")
####################################

set(GFLAGS_IS_SUBPROJECT FALSE)
# Properties->C/C++->General->Additional Include Directories
include_directories (
    ${InferenceEngine_INCLUDE_DIRS}
)

set(MSDK CACHE STRING "")
set(BUILD_TARGET CACHE STRING "")

if(BUILD_TARGET STREQUAL "BXT")
    message(STATUS "[INFO] Target platform is BXT (APL)")
    set(COMPILE_OPTION "-D${BUILD_TARGET}")
endif()
include(cmake/CPUID.cmake)
include(cmake/OptimizationFlags.cmake)
include(cmake/feature_defs.cmake)

add_subdirectory(extension)
add_subdirectory(video_analytics_example)

