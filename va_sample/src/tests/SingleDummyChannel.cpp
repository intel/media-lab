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
#include <mfxvideo++.h>
#include <stdio.h>
#include <unistd.h>


int main()
{
    DummyDecodeThread *decodeThread = new DummyDecodeThread(0);
    decodeThread->SetDecodeOutputRef(2);
    decodeThread->SetVPRatio(5);
    DummyInferenceThread *inferThread = new DummyInferenceThread(0);
    DummyTrackingThread *trackThread = new DummyTrackingThread(0);
    DummyDisplayThread *displayThread = new DummyDisplayThread();

    VAConnectorRR *c0 = new VAConnectorRR(1, 1, 10);
    VAConnectorRR *c1 = new VAConnectorRR(1, 1, 10);
    VAConnectorRR *c2 = new VAConnectorRR(1, 1, 10);

    VAConnectorPin *sink = new VASinkPin();

    //decodeThread->ConnectOutput(sink);

    decodeThread->ConnectOutput(c0->NewInputPin());
    inferThread->ConnectInput(c0->NewOutputPin());
    inferThread->ConnectOutput(c1->NewInputPin());
    trackThread->ConnectInput(c1->NewOutputPin());
    trackThread->ConnectOutput(c2->NewInputPin());
    displayThread->ConnectInput(c2->NewOutputPin());
    displayThread->ConnectOutput(sink);

    decodeThread->Prepare();
    inferThread->Prepare();
    trackThread->Prepare();
    displayThread->Prepare();

    VAThreadBlock::RunAllThreads();

    pause();
}

