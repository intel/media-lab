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
    VADataCleaner::getInstance().Initialize(true);

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
