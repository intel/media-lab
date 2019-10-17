#include "InferenceResnet50.h"
#include <ie_plugin_config.hpp>
#include <ie_plugin_ptr.hpp>
#include <cpp/ie_cnn_net_reader.h>
#include <inference_engine.hpp>

#include <ext_list.hpp>
#include <ie_plugin_dispatcher.hpp>
#include <ie_plugin_ptr.hpp>
#include <cpp/ie_cnn_net_reader.h>
#include <cpp/ie_infer_request.hpp>
#include <ie_device.hpp>

#include "DataPacket.h"

using namespace std;
using namespace InferenceEngine::details;
using namespace InferenceEngine;

InferenceResnet50::InferenceResnet50():
    m_inputWidth(0),
    m_inputHeight(0),
    m_channelNum(1),
    m_resultSize(0)
{
}

InferenceResnet50::~InferenceResnet50()
{
}

int InferenceResnet50::SetDataPorts()
{
	InferenceEngine::InputsDataMap inputInfo(m_network.getInputsInfo());
	auto& inputInfoFirst = inputInfo.begin()->second;
	inputInfoFirst->setPrecision(Precision::U8);
	inputInfoFirst->getInputData()->setLayout(Layout::NCHW);

    // ---------------------------Set outputs ------------------------------------------------------	
	InferenceEngine::OutputsDataMap outputInfo(m_network.getOutputsInfo());
	auto& _output = outputInfo.begin()->second;
	_output->setPrecision(Precision::FP32);
}

int InferenceResnet50::Load(const char *device, const char *model, const char *weights)
{
    int ret = InferenceOV::Load(device, model, weights);
    if (ret)
    {
        return ret;
    }

    // ---------------------------Set outputs ------------------------------------------------------	
    InferenceEngine::OutputsDataMap outputInfo(m_network.getOutputsInfo());
    auto& _output = outputInfo.begin()->second;
    const InferenceEngine::SizeVector outputDims = _output->getTensorDesc().getDims();
    m_resultSize = outputDims[1];

    InferenceEngine::InputsDataMap inputInfo(m_network.getInputsInfo());
    auto& inputInfoFirst = inputInfo.begin()->second;
    const InferenceEngine::SizeVector inputDims = inputInfoFirst->getTensorDesc().getDims();
    m_channelNum = inputDims[1];
    m_inputWidth = inputDims[3];
    m_inputHeight = inputDims[2];

    return 0;
}

void InferenceResnet50::GetRequirements(uint32_t *width, uint32_t *height, uint32_t *fourcc)
{
    *width = m_inputWidth;
    *height = m_inputHeight;
    *fourcc = 0x50424752; //MFX_FOURCC_RGBP
}

void InferenceResnet50::CopyImage(const uint8_t *img, void *dst, uint32_t batchIndex)
{
    uint8_t *input = (uint8_t *)dst;
    input += batchIndex * m_inputWidth * m_inputHeight * m_channelNum;
    //
    // The img in BGR format
    //cv::Mat imgMat(m_inputHeight, m_inputWidth, CV_8UC1, img);
    //std::vector<cv::Mat> inputChannels;
    //for (int i = 0; i < m_channelNum; i ++)
    //{
    //    cv::Mat channel(m_inputHeight, m_inputWidth, CV_8UC1, input + i * m_inputWidth * m_inputHeight);
    //    inputChannels.push_back(channel);
    //}
    //cv::split(imgMat, inputChannels);

    //
    // The img should already in RGBP format, if not, using the above code
    memcpy(input, img, m_channelNum * m_inputWidth * m_inputHeight);
}

int InferenceResnet50::Translate(std::vector<VAData *> &datas, uint32_t count, void *result, uint32_t *channelIds, uint32_t *frameIds, uint32_t *roiIds)
{
    float *curResult = (float *)result;

    for (int i = 0; i < count; i ++)
    {
        int c = -1;
        float conf = 0;
        for (int j = 0; j < m_resultSize; j ++)
        {
            if (curResult[j] > conf)
            {
                c = j;
                conf = curResult[j];
            }
        }
        VAData *data = VAData::Create(0, 0, 0, 0, c, conf);
        data->SetID(channelIds[i], frameIds[i]);
        // one roi creates one output, just copy the roiIds
        data->SetRoiIndex(roiIds[i]);
        datas.push_back(data);
        curResult += m_resultSize;
    }

    return 0;
}

