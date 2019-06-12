#pragma once
/*
// Copyright (c) 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include <gflags/gflags.h>
#include <algorithm>
#include <functional>
#include <iostream>
#include <fstream>
#include <random>
#include <string>
#include <vector>
#include <list>
#include <time.h>
#include <limits>
#include <chrono>

#include <opencv2/opencv.hpp>
#include <opencv2/photo/photo.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/video/video.hpp>

#define DEFAULT_PATH_P "./lib"
#define NUMLABELS 20

#ifndef OS_LIB_FOLDER
#define OS_LIB_FOLDER "/"
#endif

#define DEFAULT_PATH_P "./lib"
#define NUMLABELS 20

#ifndef OS_LIB_FOLDER
#define OS_LIB_FOLDER "/"
#endif

/// @brief message for help argument
static const char help_message[] = "Print a usage message";
/// @brief message for images argument
static const char image_message[] = "Required. Path to input video file";
/// @brief message for plugin_path argument
static const char plugin_path_message[] = "Path to a plugin folder";
/// @brief message for model argument
static const char model_message[] = "Required. Path to IR .xml file.";
/// @brief message for labels argument
static const char labels_message[] = "Required. Path to labels file.";
/// @brief message for plugin argument
static const char plugin_message[] = "Plugin name. (MKLDNNPlugin, clDNNPlugin) Force load specified plugin ";
/// @brief message for assigning cnn calculation to device
static const char target_device_message[] = "Infer target device (CPU or GPU)";
/// @brief message for inference type
static const char infer_type_message[] = "Infer type (SSD, YOLO, YOLO-tiny)";
/// @brief message for performance counters
static const char performance_counter_message[] = "Enables per-layer performance report";
/// @brief message for performance inference
static const char performance_inference_message[] = "Enables inference performance report";
/// @brief message for performance decode
static const char performance_decode_message[] = "Enables decode performance report";
/// @brief message for performance decode
static const char pipeline_latency_message[] = "Enables pipeline latency report";
/// @brief message for performance counters
static const char threshold_message[] = "confidence threshold for bounding boxes 0-1";
/// @brief message for batch size
static const char batch_message[] = "Batch size";
/// @brief message for frames count
static const char frames_message[] = "Number of frames from stream to process";
/// @brief message for channels of streams
static const char channels_message[] = "Number of channels of streams to process";
/// @brief message for frame rate of inference
static const char fps_message[] = "Number of frame rates of inference for each stream";
/// @brief message for show result
static const char show_message[] = "Show inference result in display GRID, maxium is 5 for now";
/// @brief message for show result
static const char dec_postproc_message[] = "VDBOX+SFC case: resize after decoder using direct pipe. Default - auto; use off option to do separate dec+vpp";
/// @brief message for decode 
static const char decode_message[] = "enable decode (true/false). Default - enable";
/// @brief message for inference
static const char inference_message[] = "enable inference (1/0). Default - enable";
/// @brief message for performance details
static const char perf_details_message[] = "enable performance details. Default - disable";


/// @brief message for verbose
static const char verbose_message[] = "Verbose";

/// \brief Define flag for showing help message <br>
DEFINE_bool(h, false, help_message);
/// \brief Define parameter for set video file <br>
/// It is a required parameter
DEFINE_string(i, "../../test_content/video/0.h264", image_message);
/// \brief Define parameter for set model file <br>
/// It is a required parameter
DEFINE_string(m, "../../test_content/IR/SSD_mobilenet/MobileNetSSD_deploy.xml", model_message);
/// \brief Define parameter for labels file <br>
/// It is a required parameter
DEFINE_string(l, "../../test_content/IR/SSD/pascal_voc_classes.txt", labels_message);
/// \brief Define parameter for set plugin name <br>
/// It is a required parameter
DEFINE_string(p, "", plugin_message);
/// \brief Define parameter for set path to plugins <br>
/// Default is ./lib
DEFINE_string(pp, DEFAULT_PATH_P, plugin_path_message);
/// \brief device the target device to infer on <br>
DEFINE_string(d, "GPU", target_device_message);
/// \brief device the target device to infer on <br>
DEFINE_string(t, "SSD", infer_type_message);
/// \brief resize after decoder using direct pipe
DEFINE_string(dec_postproc, "off", dec_postproc_message);
/// \brief Enable per-layer performance report
DEFINE_bool(pc, false, performance_counter_message);
/// \brief Enable per-layer performance report
DEFINE_double(thresh, .8, threshold_message);
/// \brief Batch size
DEFINE_int32(batch, 1, batch_message);
/// \brief Frames count
DEFINE_int32(fr, 256, frames_message);
/// \brief Channels of streams
DEFINE_int32(c, 1, channels_message);
/// \brief Frame rate of inference for each stream
DEFINE_int32(fps, 30, fps_message);
/// \brief Show final result
DEFINE_bool(show, false, show_message);
/// \brief Enable inference performance
DEFINE_bool(pi, false, performance_inference_message);
/// \brief Enable decode performance
DEFINE_bool(pd, false, performance_decode_message);
/// \brief Enable pipeline latency report
DEFINE_bool(pl, false, pipeline_latency_message);
/// \brief Enable inference
DEFINE_int32(infer, 1, inference_message);
/// \brief Enable inference perf details
DEFINE_bool(pv, false, perf_details_message);


/// \brief Verbose
DEFINE_bool(v, false, verbose_message);

//temporary workaround for h264 encode outliers
DEFINE_double(max_encode_ms, 40.0,NULL);

#define	VIDEO_SOURCE_MAX_STRIDE  4
#include <mfxvideo++.h>
#include <mfxstructures.h>

 /**
 * Data structure to store a video frame in RGBA or YUV420 format.
 */
typedef struct vsource_frame_s {
    int realwidth;
    int realheight;
    int realstride;
    int realsize;
    int  channel;
    int  frameno;
	
    long long imgpts;
	bool bROIRrefresh;
    int linesize[VIDEO_SOURCE_MAX_STRIDE];

    struct timeval timestamp;
    int maxstride;
    int imgbufsize;
    cv::Mat       cvImg;
    unsigned char *imgbuf;
    unsigned char *imgbuf_internal;
    mfxFrameSurface1* pmfxSurface;

    int alignment;	
}vsource_frame_t;

class VaDualPipe;
typedef struct __Infer_Task_s {
    VaDualPipe *dpipe;
    void *dbuffer;
    int       nChannelID;
}infer_task_t;


