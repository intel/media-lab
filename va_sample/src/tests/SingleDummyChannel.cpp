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

