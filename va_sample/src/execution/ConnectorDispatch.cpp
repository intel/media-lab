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

