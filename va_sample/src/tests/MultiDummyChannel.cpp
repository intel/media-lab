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
        decodeThreads[i]->Prepare();
    }

    for (int i = 0; i < inferNum; i++)
    {
        inferThreads[i]->ConnectInput(c0->NewOutputPin());
        inferThreads[i]->ConnectOutput(c1->NewInputPin());
        inferThreads[i]->Prepare();
    }

    for (int i = 0; i < trackNum; i++)
    {
        trackThreads[i]->ConnectInput(c1->NewOutputPin());
        trackThreads[i]->ConnectOutput(c2->NewInputPin());
        trackThreads[i]->Prepare();
    }
    
    displayThread->ConnectInput(c2->NewOutputPin());
    displayThread->ConnectOutput(sink);
    displayThread->Prepare();

    VAThreadBlock::RunAllThreads();

    pause();
}

