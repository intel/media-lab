#ifndef __INFERRENCE_OPENVINO_H__
#define __INFERRENCE_OPENVINO_H__

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/opencv.hpp>

#include <vector>
#include <queue>
#include <ie_plugin_config.hpp>
#include <ie_plugin_ptr.hpp>
#include <cpp/ie_cnn_net_reader.h>
#include <inference_engine.hpp>

#include "Inference.h"

class InferenceOV : public InferenceBlock
{
public:
    InferenceOV();
    virtual ~InferenceOV();

    int Initialize(uint32_t batch_num = 1, uint32_t async_depth = 1);

    // derived classes need to get input dimension and output dimension besides the base Load operation
    virtual int Load(const char *device, const char *model, const char *weights);

    // the img should already be in format that the model requests, otherwise, do the conversion outside
    int InsertImage(const uint8_t *img, uint32_t channelId, uint32_t frameId);

    int Wait();

    int GetOutput(std::vector<VAData *> &datas, std::vector<uint32_t> &channels, std::vector<uint32_t> &frames);

protected:
    // derived classes need to fill the dst with the img, based on their own different input dimension
    virtual void CopyImage(const uint8_t *img, void *dst, uint32_t batchIndex) = 0;

    // derived classes need to fill VAData by the result, based on their own different output demension
    virtual int Translate(std::vector<VAData *> &datas, uint32_t count, void *result, uint32_t *channelIds, uint32_t *frameIds) = 0;

    // derived classes need to set the input and output info
    virtual int SetDataPorts() = 0;

    int GetOutputInternal(std::vector<VAData *> &datas, std::vector<uint32_t> &channels, std::vector<uint32_t> &frames);

    std::queue<InferenceEngine::InferRequest::Ptr> m_busyRequest;
    std::queue<InferenceEngine::InferRequest::Ptr> m_freeRequest;

    std::queue<uint64_t> m_ids; // higher 32-bit: channel id, lower 32-bit: frame index

    uint32_t m_asyncDepth;
    uint32_t m_batchNum;

    uint32_t m_batchIndex;

    // model related
    std::string m_inputName;
    std::string m_outputName;

    // openvino related
    InferenceEngine::InferencePlugin m_engine;
    InferenceEngine::CNNNetwork m_network;
    InferenceEngine::ExecutableNetwork m_execNetwork;

    // internal buffer to guarantee InsertImage can always find free worker
    std::vector<VAData *> m_internalDatas;
    std::vector<uint32_t> m_internalChannels;
    std::vector<uint32_t> m_internalFrames;
};

#endif //__INFERRENCE_OPENVINO_H__
