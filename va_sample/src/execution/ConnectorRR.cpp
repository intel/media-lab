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
#include "ConnectorRR.h"
#include <unistd.h>

VAConnectorRR::VAConnectorRR(uint32_t maxInput, uint32_t maxOutput, uint32_t bufferNum):
    VAConnector(maxInput, maxOutput)
{
    uint32_t totalBufferNum = maxInput * bufferNum;
    m_packets = new VADataPacket[totalBufferNum];
    for (int i = 0; i < totalBufferNum; i++)
    {
        m_inPipe.push_back(&m_packets[i]);
    }

    pthread_mutex_init(&m_mutex, NULL);
    pthread_cond_init(&m_cond, NULL);
}

VAConnectorRR::~VAConnectorRR()
{
    if (m_packets)
    {
        delete[] m_packets;
    }
    pthread_mutex_destroy(&m_mutex);
    pthread_cond_destroy(&m_cond);
}

VADataPacket *VAConnectorRR::GetInput(int index)
{
    VADataPacket *buffer = NULL;
    do
    {
        pthread_mutex_lock(&m_mutex);
        if (m_inPipe.size() > 0)
        {
            buffer = m_inPipe.front();
            m_inPipe.pop_front();
        }
        else
        {
            pthread_mutex_unlock(&m_mutex);
            usleep(500);
        }
    } while (buffer == NULL);
    pthread_mutex_unlock(&m_mutex);
    return buffer;
}

void VAConnectorRR::StoreInput(int index, VADataPacket *data)
{
    pthread_mutex_lock(&m_mutex);
    m_outPipe.push_back(data);
    pthread_mutex_unlock(&m_mutex);
    pthread_cond_signal(&m_cond);
}

VADataPacket *VAConnectorRR::GetOutput(int index, const timespec *abstime) 
{
    bool timeout = false;
    VADataPacket *buffer = nullptr;
    pthread_mutex_lock(&m_mutex);
    do
    {
        if (m_outPipe.size() > 0)
        {
            buffer = m_outPipe.front();
            m_outPipe.pop_front();
        }
        else if (abstime == nullptr)
        {
            pthread_cond_wait(&m_cond, &m_mutex);
        }
        else if (!timeout)
        {
            pthread_cond_timedwait(&m_cond, &m_mutex, abstime);
            timeout = true;
        }
        else if (timeout)
        {
            break;
        }
    } while(buffer == NULL);
    pthread_mutex_unlock(&m_mutex);
    return buffer;
}

void VAConnectorRR::StoreOutput(int index, VADataPacket *data)
{
    pthread_mutex_lock(&m_mutex);
    m_inPipe.push_front(data);
    pthread_mutex_unlock(&m_mutex);
}

