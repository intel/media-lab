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

#include "DataPacket.h"
#include "ConnectorDispatch.h"
#include <mfxvideo++.h>
#include <stdio.h>
#include <unistd.h>

struct SenderArg
{
    VAConnectorPin *pin;
    int channelNo;
};

struct ReceiverArg
{
    VAConnectorPin *pin;
    int consumerNo;
};


void *VaSenderFunc(void *arg)
{
    SenderArg *param = static_cast<SenderArg *>(arg);
    VAConnectorPin *pin = param->pin;

    for (int i = 0; i < 10; i++)
    {
        VADataPacket *packet = pin->Get();
        if (!packet->empty())
        {
            printf("Error, packet get from input pin is not empty\n");
            return (void *)-1;
        }
        printf("Sender %d: Send packet frame %d\n", param->channelNo, i);
        VAData *data = VAData::Create(0, 0, 1280, 720); //ROI Region
        data->SetID(param->channelNo, i);
        packet->push_back(data);
        pin->Store(packet);
        usleep(1000000);
    }

    return (void *)0;
}

static int totalReceived = 0;
static pthread_mutex_t mutex;
static bool iscontinue = true;

void *VaReceiverFunc(void *arg)
{
    ReceiverArg *param = static_cast<ReceiverArg *>(arg);
    VAConnectorPin *pin = param->pin;

    while (iscontinue)
    {
        VADataPacket *packet = pin->Get();
        VAData *data = packet->front();
        printf("Receiver %d: Get packet channel %d, frame %d\n", param->consumerNo, data->ChannelIndex(), data->FrameIndex());
        data->DeRef();
        packet->clear();
        pin->Store(packet);
        pthread_mutex_lock(&mutex);
        totalReceived ++;
        pthread_mutex_unlock(&mutex);
        usleep(10000);
    }

    return (void *)0;
}

int main()
{
    VAConnectorDispatch *connector = new VAConnectorDispatch(3, 3, 5);

    SenderArg s0;
    s0.channelNo = 0;
    s0.pin = connector->NewInputPin();

    SenderArg s1;
    s1.channelNo = 1;
    s1.pin = connector->NewInputPin();

    SenderArg s2;
    s2.channelNo = 2;
    s2.pin = connector->NewInputPin();

    ReceiverArg r0;
    r0.consumerNo = 0;
    r0.pin = connector->NewOutputPin();

    ReceiverArg r1;
    r1.consumerNo = 1;
    r1.pin = connector->NewOutputPin();

    ReceiverArg r2;
    r2.consumerNo = 2;
    r2.pin = connector->NewOutputPin();

    pthread_t threads[6];

    pthread_create(&threads[0], nullptr, VaSenderFunc, (void *)&s0);
    pthread_create(&threads[1], nullptr, VaSenderFunc, (void *)&s1);
    pthread_create(&threads[2], nullptr, VaSenderFunc, (void *)&s2);

    pthread_create(&threads[3], nullptr, VaReceiverFunc, (void *)&r0);
    pthread_create(&threads[4], nullptr, VaReceiverFunc, (void *)&r1);
    pthread_create(&threads[5], nullptr, VaReceiverFunc, (void *)&r2);

    while (totalReceived != 30)
    {
    }

    printf("Stop all threads\n");
    iscontinue = false;

    pthread_join(threads[0], nullptr);
    pthread_join(threads[1], nullptr);
    pthread_join(threads[2], nullptr);
    pthread_join(threads[3], nullptr);
    pthread_join(threads[4], nullptr);
    pthread_join(threads[5], nullptr);
    
    delete connector;
    return 0;
}
