/*
// Copyright (c) 2019 Intel Corporation
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
#include "Connector.h"
#include <string.h>

VAConnector::VAConnector(uint32_t maxInput, uint32_t maxOutput):
    m_maxIn(maxInput),
    m_maxOut(maxOutput)
{
    pthread_mutex_init(&m_InputMutex, nullptr);
    pthread_mutex_init(&m_OutputMutex, nullptr);
}

VAConnector::~VAConnector()
{
    // delete all pins
    while (!m_inputPins.empty())
    {
        VAConnectorPin *pin = m_inputPins.back();
        delete pin;
        m_inputPins.pop_back();
    }
    while (!m_outputPins.empty())
    {
        VAConnectorPin *pin = m_outputPins.back();
        delete pin;
        m_outputPins.pop_back();
    }
}

VAConnectorPin *VAConnector::NewInputPin()
{
    pthread_mutex_lock(&m_InputMutex);
    VAConnectorPin *pin = nullptr;
    if (m_inputPins.size() < m_maxIn)
    {
        pin = new VAConnectorPin(this, m_inputPins.size(), true);
        m_inputPins.push_back(pin);
    }
    else
    {
        printf("Can't allocate more input pins\n");
    }
    pthread_mutex_unlock(&m_InputMutex);
    return pin;
}

VAConnectorPin *VAConnector::NewOutputPin()
{
    VAConnectorPin *pin = nullptr;
    pthread_mutex_lock(&m_OutputMutex);
    if (m_outputPins.size() < m_maxOut)
    {
        pin = new VAConnectorPin(this, m_outputPins.size(), false);
        m_outputPins.push_back(pin);
    }
    else
    {
        printf("Can't allocate more output pins\n");
    }
    pthread_mutex_unlock(&m_OutputMutex);
    return pin;
}

void VACsvWriterPin::Store(VADataPacket *data)
{
    float lefts[16];
    float tops[16];
    float rights[16];
    float bottoms[16];
    int classes[16];
    float confs[16];
    memset(lefts, 0, sizeof(lefts));
    memset(tops, 0, sizeof(tops));
    memset(rights, 0, sizeof(rights));
    memset(bottoms, 0, sizeof(bottoms));
    memset(classes, 0, sizeof(classes));
    memset(confs, 0, sizeof(confs));
    uint32_t roiNum = 0;
    uint32_t channel = 0;
    uint32_t frame = 0;
    for (auto ite = data->begin(); ite != data->end(); ite ++)
    {
        VAData *data = *ite;
        channel = data->ChannelIndex();
        frame = data->FrameIndex();

        if (data->Type() == ROI_REGION)
        {
            float left, top, right, bottom;
            data->GetRoiRegion(&left, &top, &right, &bottom);
            uint32_t index = data->RoiIndex();
            if (index > roiNum)
            {
                roiNum = index;
            }
            lefts[index] = left;
            tops[index] = top;
            rights[index] = right;
            bottoms[index] = bottom;
        }
        else if (data->Type() == IMAGENET_CLASS)
        {
            uint32_t index = data->RoiIndex();
            if (index > roiNum)
            {
                roiNum = index;
            }
            classes[index] = data->Class();
            confs[index] = data->Confidence();
        }
        data->SetRef(0);
        VADataCleaner::getInstance().Add(data);
    }
    data->clear();
    ++ roiNum;
    for (int i = 0; i < roiNum; i++)
    {
        fprintf(m_fp, "%d, %d, %d, %f, %f, %f, %f, %d, %f\n", channel,
                                                              frame,
                                                              i,
                                                              lefts[i],
                                                              tops[i],
                                                              rights[i],
                                                              bottoms[i],
                                                              classes[i],
                                                              confs[i]);
    }
}

