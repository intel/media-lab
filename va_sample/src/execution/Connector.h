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

#ifndef __CONNECTOR_H__
#define __CONNECTOR_H__
#include <stdint.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "DataPacket.h"

class VAConnectorPin;
class VAThreadBlock;

class VAConnector
{
friend class VAConnectorPin;
public:
    VAConnector(uint32_t maxInput, uint32_t maxOutput);
    virtual ~VAConnector();
    
    VAConnectorPin *NewInputPin();
    VAConnectorPin *NewOutputPin();
    
protected:
    virtual VADataPacket *GetInput(int index) = 0;
    virtual void StoreInput(int index, VADataPacket *data) = 0;
    virtual VADataPacket *GetOutput(int index, const timespec *abstime = nullptr) = 0;
    virtual void StoreOutput(int index, VADataPacket *data) = 0;
    std::vector<VAConnectorPin *> m_inputPins;
    std::vector<VAConnectorPin *> m_outputPins;

    uint32_t m_maxIn;
    uint32_t m_maxOut;

    pthread_mutex_t m_InputMutex;
    pthread_mutex_t m_OutputMutex;
};

class VAConnectorPin
{
public:
    VAConnectorPin(VAConnector *connector, int index, bool isInput):
        m_connector(connector),
        m_index(index),
        m_isInput(isInput)
    {
        printf("pin index %d\n", m_index);
    }

    virtual ~VAConnectorPin() {}
    
    virtual VADataPacket *Get()
    {
        if (m_isInput)
            return m_connector->GetInput(m_index);
        else
            return m_connector->GetOutput(m_index);
    }
    
    virtual void Store(VADataPacket *data)
    {
        if (m_isInput)
            m_connector->StoreInput(m_index, data);
        else
            m_connector->StoreOutput(m_index, data);
    }

protected:
    VAConnector *m_connector;
    const int m_index;
    bool m_isInput;
};

class VASinkPin : public VAConnectorPin
{
public:
    VASinkPin():
        VAConnectorPin(nullptr, 0, true) {}

    VADataPacket *Get()
    {
        return &m_packet;
    }

    void Store(VADataPacket *data)
    {
        for (auto ite = data->begin(); ite != data->end(); ite ++)
        {
            VAData *data = *ite;
            data->SetRef(0);
            printf("Warning: in sink, still referenced data: %p, channel %d, frame %d\n", data, data->ChannelIndex(), data->FrameIndex());
            VADataCleaner::getInstance().Add(data);
        }
        data->clear();
    }
protected:
    VADataPacket m_packet;
};

class VAFilePin: public VAConnectorPin
{
public:
    VAFilePin(const char *filename):
        VAConnectorPin(nullptr, 0, false),
        m_fp(nullptr)
    {
        m_fp = fopen(filename, "rb");
        fseek(m_fp, 0, SEEK_SET);
        m_dataSize = 1024*1024;
        m_data = (uint8_t *)malloc(m_dataSize);
    }

    ~VAFilePin()
    {
        free(m_data);
    }

    VADataPacket *Get()
    {
        uint32_t num = fread(m_data, 1, m_dataSize, m_fp);
        if (num == 0)
        {
            fseek(m_fp, 0, SEEK_SET);
            num = fread(m_data, 1, m_dataSize, m_fp);
        }

        VAData *data = VAData::Create(m_data, 0, num);
        m_packet.push_back(data);
        return &m_packet;
    }

    void Store(VADataPacket *data)
    {
        if (data != &m_packet)
        {
            printf("Error: store a wrong packet in VAFilePin\n");
        }
    }

protected:
    FILE *m_fp;
    uint8_t *m_data;
    uint32_t m_dataSize;

    VADataPacket m_packet;
};

#endif
