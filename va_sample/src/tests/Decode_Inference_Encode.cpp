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

#include "common.h"
#include "DecodeThreadBlock.h"
#include "EncodeThreadBlock.h"
#include "InferenceThreadBlock.h"
#include "ConnectorRR.h"
#include "Statistics.h"

#include <mfxvideo++.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <string>
#include <sstream>
#include <memory>

static int vpp_w = 300;
static int vpp_h = 300;
std::string input_filename;
static int vpp_output_fourcc = MFX_FOURCC_NV12;

void App_ShowUsage(void)
{
    printf("Usage: Decode_Inference_Encode -i input.264\n");
}

void ParseOpt(int argc, char *argv[])
{
    if (argc < 2)
    {
        App_ShowUsage();
        exit(0);
    }
    std::vector <std::string> sources;
    std::string arg = argv[1];
    if ((arg == "-h") || (arg == "--help"))
    {
        App_ShowUsage();
        exit(0);
    }
    for (int i = 1; i < argc; ++i)
        sources.push_back(argv[i]);

    for (int i = 0; i < argc-1; ++i)
    {
        if (sources.at(i) == "-i")
            input_filename = sources.at(++i);
    }

    if (input_filename.empty())
    {
        printf("Missing input file name!!!!!!!\n");
        App_ShowUsage();
        exit(0);
    }
}

int main(int argc, char *argv[])
{
    ParseOpt(argc, argv);

    DecodeThreadBlock *d = new DecodeThreadBlock(0);
    InferenceThreadBlock *infer = new InferenceThreadBlock(0, MOBILENET_SSD_U8);
    EncodeThreadBlock *e = new EncodeThreadBlock(0, VAEncodeAvc);
    VAFilePin *pin = new VAFilePin(input_filename.c_str());
    VASinkPin *sink = new VASinkPin();
    VAConnectorRR *c1 = new VAConnectorRR(1, 1, 10);
    VAConnectorRR *c2 = new VAConnectorRR(1, 1, 10);

    d->ConnectInput(pin);
    d->ConnectOutput(c1->NewInputPin());
    d->SetDecodeOutputRef(1); // decode output surface feeded to encoder
    d->SetVPOutputRef(1); // vp output surface feeded to inference
    d->SetVPRatio(1);
    d->SetVPOutResolution(300, 300);
    d->Prepare();
    uint32_t w, h;
    d->GetDecodeResolution(&w, &h);

    infer->ConnectInput(c1->NewOutputPin());
    infer->ConnectOutput(c2->NewInputPin());
    infer->SetAsyncDepth(1);
    infer->SetBatchNum(1);
    infer->SetDevice("GPU");
    infer->SetModelFile("../../models/mobilenet-ssd.xml", "../../models/mobilenet-ssd.bin");
    infer->Prepare();

    e->ConnectInput(c2->NewOutputPin());
    e->ConnectOutput(sink);
    e->SetAsyncDepth(1);
    e->SetInputFormat(MFX_FOURCC_NV12);
    e->SetInputResolution(w, h);
    e->SetOutputRef(0); // not following blocks will use encoder output
    e->SetEncodeOutDump(true);
    e->Prepare();

    VAThreadBlock::RunAllThreads();

    Statistics::getInstance().ReportPeriodly(1.0);

    VAThreadBlock::StopAllThreads();
    delete d;
    delete infer;
    delete e;

}




