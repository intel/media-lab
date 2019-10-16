#include "InferenceThreadBlock.h"
#include "Inference.h"
#include <queue>
#include <map>
#include <iostream>

InferenceThreadBlock::InferenceThreadBlock(uint32_t index, InferenceModelType type):
    m_index(index),
    m_type(type),
    m_asyncDepth(1),
    m_batchNum(1),
    m_device(nullptr),
    m_infer(nullptr)
{
}

InferenceThreadBlock::~InferenceThreadBlock()
{
    if (m_infer)
    {
        InferenceBlock::Destroy(m_infer);
    }
}

int InferenceThreadBlock::Prepare()
{
    m_infer = InferenceBlock::Create(m_type);
    m_infer->Initialize(m_batchNum, m_asyncDepth);
    int ret = m_infer->Load(m_device, m_modelFile, m_weightsFile);
    if (ret)
    {
        std::cout << "load model failed!\n";
        return -1;
    }

    return 0;
}

bool InferenceThreadBlock::CanBeProcessed(VAData *data)
{
    if (data->Type() != USER_SURFACE)
    {
        return false;
    }
    uint32_t w, h, p, format;
    data->GetSurfaceInfo(&w, &h, &p, &format);

    uint32_t rw, rh, rf;
    m_infer->GetRequirements(&rw, &rh, &rf);

    return w == rw && p == rw && h == rh && format == rf;
}

int InferenceThreadBlock::Loop()
{
    std::queue<uint64_t> hasOutputs;
    std::map<uint64_t, VADataPacket *> recordedPackets;
    while (true)
    {
        VADataPacket *InPacket = AcquireInput();
        VADataPacket *tempPacket = new VADataPacket;

        // get all the inference inputs
        std::vector<VAData *>vpOuts;
        for (auto ite = InPacket->begin(); ite != InPacket->end(); ite++)
        {
            VAData *data = *ite;
            if (CanBeProcessed(data))
            {
                vpOuts.push_back(data);
            }
            else
            {
                tempPacket->push_back(data);
            }
        }
        ReleaseInput(InPacket);

        // insert the images to inference engine
        if (vpOuts.size() > 0)
        {
            recordedPackets[ID(vpOuts[0]->ChannelIndex(), vpOuts[0]->FrameIndex())] = tempPacket;
        }
        for (int i = 0; i < vpOuts.size(); i ++)
        {
            m_infer->InsertImage(vpOuts[i]->GetSurfacePointer(), vpOuts[i]->ChannelIndex(), vpOuts[i]->FrameIndex());
            vpOuts[i]->DeRef(tempPacket);
        }

        // get all avalible inference output
        std::vector<VAData *> outputs;
        std::vector<uint32_t> channels;
        std::vector<uint32_t> frames;
        uint32_t lastSize = 0;
        bool inferenceFree = false;
        // get available outputs
        while (1)
        {
            int ret = m_infer->GetOutput(outputs, channels, frames);
            //printf("%d outputs\n", outputs.size());
            if (ret == -1)
            {
                inferenceFree = true;
            }

            if (outputs.size() == lastSize)
            {
                break;
            }
            else
            {
                lastSize = outputs.size();
            }
        }

        // insert the inference outputs to the packets
        int j = 0;
        for (int i = 0; i < channels.size(); i++)
        {
            VADataPacket *targetPacket = recordedPackets[ID(channels[i], frames[i])];

            if (hasOutputs.size() == 0 || ID(channels[i], frames[i]) != hasOutputs.back())
            {
                hasOutputs.push(ID(channels[i], frames[i]));
            }
            
            while (j < outputs.size()
                    && outputs[j]->ChannelIndex() == channels[i]
                    && outputs[j]->FrameIndex() == frames[i])
            {
                targetPacket->push_back(outputs[j]);
                ++j;
            }
        }

        // send the packets to next block
        if (hasOutputs.size() == 0)
        {
            continue;
        }
        uint32_t outputNum = hasOutputs.size() - 1; // can't enqueue last one because it may has following outputs in next GetOutput
        if (inferenceFree)
        {
            ++ outputNum; // no inference task now, so the last one can also be enqueued
        }
        for (int i = 0; i < outputNum; i++)
        {
            uint64_t id = hasOutputs.front();
            hasOutputs.pop();
            VADataPacket *targetPacket = recordedPackets[id];
            recordedPackets.erase(id);
            VADataPacket *outputPacket = DequeueOutput();
            outputPacket->insert(outputPacket->end(), targetPacket->begin(), targetPacket->end());
            delete targetPacket;
            EnqueueOutput(outputPacket);
        }
    }
    return 0;
}

