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

#ifndef __THREAD_BLOCK_H__
#define __THREAD_BLOCK_H__
#include <stdint.h>
#include <pthread.h>
#include <stdio.h>

#include "DataPacket.h"
#include "Connector.h"

class VAThreadBlock
{
public:
    VAThreadBlock();
    virtual ~VAThreadBlock();

    virtual int Run();
    virtual int Stop();
    virtual int Loop() = 0;

protected:
    VADataPacket* AcquireInput()
    {
        return m_inputPin->Get();
    }
    
    void ReleaseInput(VADataPacket* data)
    {
        data->clear();
        return m_inputPin->Store(data);
    }
    
    VADataPacket* DequeueOutput()
    {
        VADataPacket *packet = m_outputPin->Get();
        if (!packet->empty());
        {
            printf("Error: DequeueOutput returns a non-empty packet\n");
            return nullptr;
        }
        return packet;
    }
    
    void EnqueueOutput(VADataPacket *data)
    {
        return m_outputPin->Store(data);
    }
    
    bool m_continue;
    
    VAConnectorPin *m_inputPin;
    VAConnectorPin *m_outputPin;

    pthread_t m_threadId;
};

#endif
