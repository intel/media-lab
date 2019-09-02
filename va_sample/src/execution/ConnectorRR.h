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

#ifndef __CONNECTOR_RR_H__
#define __CONNECTOR_RR_H__

#include "Connector.h"

class VAConnectorRR : public VAConnector
{
public:
    VAConnectorRR(uint32_t maxInput, uint32_t maxOutput, uint32_t bufferNum);
    ~VAConnectorRR();
    
protected:
    virtual VADataPacket *GetInput(int index) override;
    virtual void StoreInput(int index, VADataPacket *data) override;
    virtual VADataPacket *GetOutput(int index, const timespec *abstime) override;
    virtual void StoreOutput(int index, VADataPacket *data) override;

    VADataPacket *m_packets;
    std::list<VADataPacket *> m_inPipe;
    std::list<VADataPacket *> m_outPipe;

    pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
};

#endif
