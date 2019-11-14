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

#include <stdio.h>
#include <string>

#include "DataPacket.h"
#include "ConnectorRR.h"
#include "InferenceThreadBlock.h"
#include "DecodeThreadBlock.h"
#include "Statistics.h"

const std::string input_file = "/home/hefan/workspace/VA/video_analytics_Intel_GPU/test_content/video/0.h264";

int main(int argc, char *argv[])
{
    int channel_num = 1;
    DecodeThreadBlock **decodeBlocks = new DecodeThreadBlock *[channel_num];
    InferenceThreadBlock **inferBlocks = new InferenceThreadBlock *[channel_num];
    VAConnectorRR **connectors = new VAConnectorRR *[channel_num];
    VAFilePin **filePins = new VAFilePin *[channel_num];
    VASinkPin **sinks = new VASinkPin *[channel_num];

    for (int i = 0; i < channel_num; i++)
    {
        DecodeThreadBlock *dec = decodeBlocks[i] = new DecodeThreadBlock(i);
        InferenceThreadBlock *infer = inferBlocks[i] = new InferenceThreadBlock(i, MOBILENET_SSD_U8);
        VAConnectorRR *c1 = connectors[i] = new VAConnectorRR(1, 1, 10);
        VAFilePin *pin = filePins[i] = new VAFilePin(input_file.c_str());
        VASinkPin *sink = sinks[i] = new VASinkPin();

        dec->ConnectInput(pin);
        dec->ConnectOutput(c1->NewInputPin());
        dec->SetDecodeOutputRef(0);
        dec->SetVPRatio(1);
        dec->SetVPOutResolution(300, 300);

        infer->ConnectInput(c1->NewOutputPin());
        infer->ConnectOutput(sink);

        infer->SetAsyncDepth(1);
        infer->SetBatchNum(1);
        infer->SetDevice("GPU");
        infer->SetModelFile("../../models/mobilenet-ssd.xml", "../../models/mobilenet-ssd.bin");

        dec->Prepare();
        infer->Prepare();
    }

    VAThreadBlock::RunAllThreads();

    Statistics::getInstance().ReportPeriodly(1.0);

    VAThreadBlock::StopAllThreads();

    for (int i = 0; i < channel_num; i++)
    {
        delete decodeBlocks[i];
        delete inferBlocks[i];
    }

    return 0;
}