/*
* Copyright (c) 2019, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
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
            printf("Warning: in sink, still referenced data: %d, channel %d, frame %d\n", data->Type(), data->ChannelIndex(), data->FrameIndex());
            data->SetRef(0);
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
        fclose(m_fp);
    }

    VADataPacket *Get()
    {
        uint32_t num = fread(m_data, 1, m_dataSize, m_fp);
        if (num == 0)
        {
            fseek(m_fp, 0, SEEK_SET);
            //num = fread(m_data, 1, m_dataSize, m_fp);
            // a VAData with size 0 means the stream is to the end
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

class VACsvWriterPin : public VAConnectorPin
{
public:
    VACsvWriterPin(const char *filename):
        VAConnectorPin(nullptr, 0, true),
        m_fp(nullptr)
    {
        m_fp = fopen(filename, "w");
    }

    ~VACsvWriterPin()
    {
        if (m_fp)
        {
            fclose(m_fp);
        }
    }

    VADataPacket *Get()
    {
        return &m_packet;
    }

    void Store(VADataPacket *data);

protected:
    VADataPacket m_packet;
    FILE *m_fp;
};

#endif
