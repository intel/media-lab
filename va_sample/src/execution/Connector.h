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
    }

    ~VAConnectorPin() {}
    
    VADataPacket *Get()
    {
        if (m_isInput)
            return m_connector->GetInput(m_index);
        else
            return m_connector->GetOutput(m_index);
    }
    
    void Store(VADataPacket *data)
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

#endif
