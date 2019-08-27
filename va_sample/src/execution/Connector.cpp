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

