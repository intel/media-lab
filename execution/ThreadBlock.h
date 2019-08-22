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

class VAConnectorPinLeft;
class VAConnectorPinRight;
class VADataPacket;

enum VAThreadBlockStatus
{
    PREPARING,
    RUNNING,
    WAITING
};

class VAThreadBlock
{
public:
    VAThreadBlock();
    virtual ~VAThreadBlock();
    
    int Initialize();

    int TearDown();
    
    void ConnectInputTo(VAConnectorPin *pin)
    {
        m_inputPin = pin;
    }
    
    void ConnectOutputTo(VAConnectorPin *pin)
    {
        m_outputPin = pin;
    }

    virtual int Run();
    {
        while(m_continue)
        {
            Loop();
        }
    }
    
    virtual int Stop();
    
    virtual int Loop() = 0;
    // example of derived classes
    #Derived loop()
    {
        VADataPacket *input = AcquireInput();
        VADataPacket *output = DequeueOutput();
        for (int i = 0; i < input->size(); i++)
        {
            if (AbleToProcess(input[i])) // check type, resolution, format ...
            {
                // process and generate an output
                VaData *data = process(input[i]); // VaData 
                output->append(data);
            }
            else
            {
                output->append(input[i]);
            }
        }
    }

protected:
    VADataPacket* AcquireInput()
    {
        return m_inputPin->Get();
    }
    
    int ReleaseInput(VADataPacket* data)
    {
        data->Clear();
        return m_inputPin->Store(data);
    }
    
    VADataPacket* DequeueOutput()
    {
        VaDataPacket *packet = m_outputPin->Get();
        CHECK(packet->IsEmpty());
        return packet;
    }
    
    int EnqueueOutput(VADataPacket *data)
    {
        return m_outputPin->Store(data);
    }
    
    bool m_continue;
    pthread_mutex_t m_mutex;
    
    VAConnectorPin *m_inputPin;
    VAConnectorPin *m_outputPin;
    
    VAThreadBlockStatus m_status;
};

#endif
