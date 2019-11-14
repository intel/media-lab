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

#ifndef __CONNECTOR_DISPATCH_H__
#define __CONNECTOR_DISPATCH_H__

#include "Connector.h"

class VAConnectorDispatch : public VAConnector
{
public:
    VAConnectorDispatch(uint32_t maxInput, uint32_t maxOutput, uint32_t bufferNum);
    ~VAConnectorDispatch();
    
protected:
    virtual VADataPacket *GetInput(int index) override;
    virtual void StoreInput(int index, VADataPacket *data) override;
    virtual VADataPacket *GetOutput(int index, const timespec *abstime) override;
    virtual void StoreOutput(int index, VADataPacket *data) override;

    VADataPacket *m_packets;

    typedef std::list<VADataPacket *> PacketList;
    PacketList m_inPipe;
    PacketList *m_outPipes;

    pthread_mutex_t m_inMutex;
    pthread_mutex_t *m_outMutex;
    pthread_cond_t *m_conds;
};

#endif
