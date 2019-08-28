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
#include "DummyThreadBlocks.h"
#include "DataPacket.h"
#include "ConnectorRR.h"
#include "ConnectorDispatch.h"
#include <mfxvideo++.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


int main()
{
    VADataCleaner::getInstance().Initialize(true);

    uint32_t decodeNum = 60;
    uint32_t inferNum = 2;
    uint32_t trackNum = 20;

    DummyDecodeThread **decodeThreads = (DummyDecodeThread **)malloc(decodeNum * sizeof(DummyDecodeThread *));
    DummyInferenceThread **inferThreads = (DummyInferenceThread **)malloc(inferNum * sizeof(DummyInferenceThread *));
    DummyTrackingThread **trackThreads = (DummyTrackingThread **)malloc(trackNum * sizeof(DummyTrackingThread *));

    for (int i = 0; i < decodeNum; i ++)
    {
        decodeThreads[i] = new DummyDecodeThread(i);
        decodeThreads[i]->SetDecodeOutputRef(2);
        decodeThreads[i]->SetVPRatio(5);
    }

    for (int i = 0; i < inferNum; i++)
    {
        inferThreads[i] = new DummyInferenceThread(i);
    }

    for (int i = 0; i < trackNum; i++)
    {
        trackThreads[i] = new DummyTrackingThread(i);
    }
    
    DummyDisplayThread *displayThread = new DummyDisplayThread();

    VAConnectorRR *c0 = new VAConnectorRR(decodeNum, inferNum, 10);
    VAConnectorDispatch *c1 = new VAConnectorDispatch(inferNum, trackNum, 10);
    VAConnectorRR *c2 = new VAConnectorRR(trackNum, 1, 10);

    VAConnectorPin *sink = new VASinkPin();

    //decodeThread->ConnectOutput(sink);

    for (int i = 0; i < decodeNum; i ++)
    {
        decodeThreads[i]->ConnectOutput(c0->NewInputPin());
    }

    for (int i = 0; i < inferNum; i++)
    {
        inferThreads[i]->ConnectInput(c0->NewOutputPin());
        inferThreads[i]->ConnectOutput(c1->NewInputPin());
    }

    for (int i = 0; i < trackNum; i++)
    {
        trackThreads[i]->ConnectInput(c1->NewOutputPin());
        trackThreads[i]->ConnectOutput(c2->NewInputPin());
    }
    
    displayThread->ConnectInput(c2->NewOutputPin());
    displayThread->ConnectOutput(sink);

    for (int i = 0; i < decodeNum; i ++)
    {
        decodeThreads[i]->Run();
    }

    for (int i = 0; i < inferNum; i++)
    {
        inferThreads[i]->Run();
    }

    for (int i = 0; i < trackNum; i++)
    {
        trackThreads[i]->Run();
    }
    displayThread->Run();

    pause();
}

