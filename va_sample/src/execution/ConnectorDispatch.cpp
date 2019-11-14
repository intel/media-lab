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

#include "ConnectorDispatch.h"
#include <unistd.h>

VAConnectorDispatch::VAConnectorDispatch(uint32_t maxInput, uint32_t maxOutput, uint32_t bufferNum):
    VAConnector(maxInput, maxOutput)
{
    uint32_t totalBufferNum = maxOutput * bufferNum;
    m_packets = new VADataPacket[totalBufferNum];
    for (int i = 0; i < totalBufferNum; i++)
    {
        m_inPipe.push_back(&m_packets[i]);
    }

    pthread_mutex_init(&m_inMutex, NULL);

    m_outMutex = new pthread_mutex_t[maxOutput];
    m_conds = new pthread_cond_t[maxOutput];
    for (int i = 0; i < maxOutput; i++)
    {
        pthread_mutex_init(&m_outMutex[i], NULL);
        pthread_cond_init(&m_conds[i], NULL);
    }

    m_outPipes = new PacketList[maxOutput];
}

VAConnectorDispatch::~VAConnectorDispatch()
{
    if (m_packets)
    {
        delete[] m_packets;
    }
    for (int i = 0; i < m_maxOut; i++)
    {
        pthread_mutex_destroy(&m_outMutex[i]);
        pthread_cond_destroy(&m_conds[i]);
    }

    delete[] m_outMutex;
    delete[] m_conds;
}

VADataPacket *VAConnectorDispatch::GetInput(int index)
{
    VADataPacket *buffer = NULL;
    do
    {
        pthread_mutex_lock(&m_inMutex);
        if (m_inPipe.size() > 0)
        {
            buffer = m_inPipe.front();
            m_inPipe.pop_front();
        }
        else
        {
            pthread_mutex_unlock(&m_inMutex);
            usleep(500);
        }
    } while (buffer == NULL);
    pthread_mutex_unlock(&m_inMutex);
    return buffer;
}

void VAConnectorDispatch::StoreInput(int index, VADataPacket *data)
{
    uint32_t channel = data->front()->ChannelIndex();
    uint32_t outIndex = channel%m_maxOut;
    pthread_mutex_lock(&m_outMutex[outIndex]);
    m_outPipes[outIndex].push_back(data);
    pthread_mutex_unlock(&m_outMutex[outIndex]);
    pthread_cond_signal(&m_conds[outIndex]);
}

VADataPacket *VAConnectorDispatch::GetOutput(int index, const timespec *abstime) 
{
    bool timeout = false;
    VADataPacket *buffer = nullptr;
    pthread_mutex_lock(&m_outMutex[index]);
    do
    {
        if (m_outPipes[index].size() > 0)
        {
            buffer = m_outPipes[index].front();
            
            m_outPipes[index].pop_front();
        }
        else if (abstime == nullptr)
        {
            pthread_cond_wait(&m_conds[index], &m_outMutex[index]);
        }
        else if (!timeout)
        {
            pthread_cond_timedwait(&m_conds[index], &m_outMutex[index], abstime);
            timeout = true;
        }
        else if (timeout)
        {
            break;
        }
    } while(buffer == NULL);
    pthread_mutex_unlock(&m_outMutex[index]);
    return buffer;
}

void VAConnectorDispatch::StoreOutput(int index, VADataPacket *data)
{
    pthread_mutex_lock(&m_inMutex);
    m_inPipe.push_front(data);
    pthread_mutex_unlock(&m_inMutex);
}

