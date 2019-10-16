#include "InferenceOV.h"
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

using namespace std;
using namespace InferenceEngine::details;
using namespace InferenceEngine;

InferenceOV::InferenceOV():
    m_asyncDepth(1),
    m_batchNum(1),
    m_batchIndex(0)
{
}

InferenceOV::~InferenceOV()
{
    while (m_busyRequest.size() != 0)
    {
        m_busyRequest.pop();
    }
    while (m_freeRequest.size() != 0)
    {
        m_freeRequest.pop();
    }
}

int InferenceOV::Initialize(uint32_t batch_num, uint32_t async_depth)
{
    m_batchNum = batch_num;
    m_asyncDepth = async_depth;
}

int InferenceOV::Load(const char *device, const char *model, const char *weights)
{
    try {
        m_engine = PluginDispatcher({ "" }).getPluginByDevice(device);
    }
    catch (InferenceEngineException e) {
        cout<<"  can not find pluginDevice"<<endl;
        return -1;
    }

    /** Loading default extensions **/
    string deviceStr(device);
    if (deviceStr.find("CPU") != std::string::npos) {
        /**
            * cpu_extensions library is compiled from "extension" folder containing
            * custom MKLDNNPlugin layer implementations. These layers are not supported
            * by mkldnn, but they can be useful for inferring custom topologies.
        **/
        m_engine.AddExtension(std::make_shared<Extensions::Cpu::CpuExtensions>());
    }

    /** Read network model **/ 
	InferenceEngine::CNNNetReader netReader;
	try {
        string modelStr(model);
        string weightsStr(weights);
		netReader.ReadNetwork(modelStr);
		netReader.ReadWeights(weightsStr);
	}
	catch (InferenceEngineException e) {
		std::cout<<"  can not load model:"<<model<<std::endl;
		return -1;
	}
    m_network = netReader.getNetwork();
    m_network.setBatchSize(m_batchNum);
    m_batchNum = m_network.getBatchSize();
    printf("Batch number get from network is %d\n", m_batchNum);

    SetDataPorts();

    // ---------------------------Set inputs ------------------------------------------------------	
	InferenceEngine::InputsDataMap inputInfo(m_network.getInputsInfo());
	auto& inputInfoFirst = inputInfo.begin()->second;
	m_inputName = inputInfo.begin()->first;

    // ---------------------------Set outputs ------------------------------------------------------	
	InferenceEngine::OutputsDataMap outputInfo(m_network.getOutputsInfo());
	auto& _output = outputInfo.begin()->second;
	m_outputName = outputInfo.begin()->first;

    // -------------------------Loading model to the plugin-------------------------------------------------
	//InferenceEngine::ExecutableNetwork exenet;
	try {
		m_execNetwork= m_engine.LoadNetwork(m_network, {});
	}
	catch (InferenceEngineException e) {
		std::cout<<"   Input Model file"<< model<<" doesn't support by current device:"<<device<<std::endl;
		return -1;
	}

    for (int i = 0; i < m_batchNum; i++)
    {
        InferRequest::Ptr request = m_execNetwork.CreateInferRequestPtr();
        m_freeRequest.push(request);
    }

    return 0;
}

int InferenceOV::InsertImage(const uint8_t *img, uint32_t channelId, uint32_t frameId)
{
    if (m_freeRequest.size() == 0)
    {
        //std::cout << "Warning: No free worker in Inference now" << std::endl;
        Wait();
        GetOutputInternal(m_internalDatas, m_internalChannels, m_internalFrames);
    }

    InferRequest::Ptr curRequest = m_freeRequest.front();
    void *dst = curRequest->GetBlob(m_inputName)->buffer();

    CopyImage(img, dst, m_batchIndex);

    uint64_t id = frameId | ((uint64_t)channelId << 32);
    m_ids.push(id);

    ++ m_batchIndex;
    if (m_batchIndex >= m_batchNum)
    {
        m_batchIndex = 0;
        curRequest->StartAsync();
        m_busyRequest.push(curRequest);
        m_freeRequest.pop();
    }

    return 0;
}

int InferenceOV::Wait()
{
    if (m_busyRequest.size() == 0)
    {
        return -1;
    }

    InferRequest::Ptr curRequest = m_busyRequest.front();
    InferenceEngine::StatusCode ret = curRequest->Wait(IInferRequest::WaitMode::RESULT_READY);
    if (ret == InferenceEngine::OK)
    {
        return 0;
    }
    else
    {
        return ret;
    }
}

int InferenceOV::GetOutput(std::vector<VAData *> &datas, std::vector<uint32_t> &channels, std::vector<uint32_t> &frames)
{
    if (m_internalDatas.size() > 0 && m_internalChannels.size() > 0 && m_internalFrames.size() > 0)
    {
        datas.insert(datas.end(), m_internalDatas.begin(), m_internalDatas.end());
        m_internalDatas.clear();
        channels.insert(channels.end(), m_internalChannels.begin(), m_internalChannels.end());
        m_internalChannels.clear();
        frames.insert(frames.end(), m_internalFrames.begin(), m_internalFrames.end());
        m_internalFrames.clear();
    }
    return GetOutputInternal(datas, channels, frames);
}

int InferenceOV::GetOutputInternal(std::vector<VAData *> &datas, std::vector<uint32_t> &channels, std::vector<uint32_t> &frames)
{
    if (m_busyRequest.size() == 0)
    {
        return -1;
    }
    uint32_t *channelIds = new uint32_t[m_batchNum];
    uint32_t *frameIds = new uint32_t[m_batchNum];
    while (m_busyRequest.size() > 0)
    {
        InferRequest::Ptr curRequest = m_busyRequest.front();
        InferenceEngine::StatusCode status;

        status = curRequest->Wait(IInferRequest::WaitMode::STATUS_ONLY);

        if (status != InferenceEngine::OK)
        {
            break;
        }
        status = curRequest->Wait(IInferRequest::WaitMode::RESULT_READY);

        if (status != InferenceEngine::OK)
        {
            break;
        }
        for (int i = 0; i < m_batchNum; i ++)
        {
            uint64_t id = m_ids.front();
            m_ids.pop();
            channelIds[i] = (id & 0xFFFFFFFF00000000) >> 32;
            frameIds[i] = (id & 0x00000000FFFFFFFF);
        }
        // curRequest finished
        const float *result = curRequest->GetBlob(m_outputName)->buffer().as<PrecisionTrait<Precision::FP32>::value_type*>();
        int ret = Translate(datas, m_batchNum, (void *)result, channelIds, frameIds);
        for (int i = 0; i < m_batchNum; i ++)
        {
            channels.push_back(channelIds[i]);
            frames.push_back(frameIds[i]);
        }

        m_freeRequest.push(curRequest);
        m_busyRequest.pop();
    }

    delete[] channelIds;
    delete[] frameIds;
    return 0;
}

