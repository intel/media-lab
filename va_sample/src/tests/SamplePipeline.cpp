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
#include "CropThreadBlock.h"
#include "DecodeThreadBlock.h"
#include "InferenceThreadBlock.h"
#include "Statistics.h"


static int vp_ratio = 1;
static int channel_num = 1;
std::string input_filename;
static int dump_crop = false;
static int inference_num = 1;
static int crop_num = 1;
static int classification_num = 1;
static int duration = -1;

void App_ShowUsage(void)
{
    printf("Usage: multichannel_decode -i input.264 [-r vp_ratio] [-c channel_number] [-d]\n");
    printf("           -r vp_ratio: the ratio of decoded frames to vp frames, e.g., -r 2 means doing vp every other frame\n");
    printf("                        5 by default\n");
    printf("                        0 means no vp\n");
    printf("           -c channel_number: number of decoding channels, default value is 1\n");
    printf("           -d dump the crop output, default value is off\n");
    printf("           -ssd ssd thread number\n");
    printf("           -crop crop thread number\n");
    printf("           -resnet resnet thread number\n");
}

int ParseOpt(int argc, char *argv[])
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
        if (sources.at(i) == "-c")
            channel_num = stoi(sources.at(++i));
        else if (sources.at(i) == "-i")
            input_filename = sources.at(++i);
        else if (sources.at(i) == "-r")
            vp_ratio = stoi(sources.at(++i));
        else if (sources.at(i) == "-d")
            dump_crop = true;
        else if (sources.at(i) == "-ssd")
            inference_num = stoi(sources.at(++i));
        else if (sources.at(i) == "-crop")
            crop_num = stoi(sources.at(++i));
        else if (sources.at(i) == "-resnet")
            classification_num = stoi(sources.at(++i));
        else if (sources.at(i) == "-t")
            duration = stoi(sources.at(++i));
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

    DecodeThreadBlock **decodeBlocks = new DecodeThreadBlock *[channel_num];
    InferenceThreadBlock **inferBlocks = new InferenceThreadBlock *[inference_num];
    CropThreadBlock **cropBlocks = new CropThreadBlock *[channel_num];
    InferenceThreadBlock **classBlocks = new InferenceThreadBlock *[classification_num];

    VAFilePin **filePins = new VAFilePin *[channel_num];
    VASinkPin **sinks = new VASinkPin *[classification_num];

    VAConnectorRR *c1 = new VAConnectorRR(channel_num, inference_num, 10);
    VAConnectorRR *c2 = new VAConnectorRR(inference_num, crop_num, 10);
    VAConnectorRR *c3 = new VAConnectorRR(crop_num, classification_num, 10);

    for (int i = 0; i < channel_num; i++)
    {
        DecodeThreadBlock *dec = decodeBlocks[i] = new DecodeThreadBlock(i);
        VAFilePin *pin = nullptr;
        pin = filePins[i] = new VAFilePin(input_filename.c_str());
        dec->ConnectInput(pin);
        dec->ConnectOutput(c1->NewInputPin());
        dec->SetDecodeOutputRef(0);
        dec->SetVPOutputRef(1);
        dec->SetDecodeOutputWithVP(); // when there is vp output, decode output also attached
        dec->SetVPRatio(vp_ratio);
        dec->SetVPOutResolution(300, 300);
        dec->Prepare();
    }
    printf("After decoder prepare\n");

    for (int i = 0; i < inference_num; i++)
    {
        InferenceThreadBlock *infer = inferBlocks[i] = new InferenceThreadBlock(i, MOBILENET_SSD_U8);
        infer->ConnectInput(c1->NewOutputPin());
        infer->ConnectOutput(c2->NewInputPin());
        infer->SetAsyncDepth(1);
        infer->SetBatchNum(1);
        infer->SetOutputRef(2);
        infer->SetDevice("GPU");
        infer->SetModelFile("../../models/mobilenet-ssd.xml", "../../models/mobilenet-ssd.bin");
        infer->Prepare();
    }
    printf("After inference prepare\n");

    for (int i = 0; i < crop_num; i++)
    {
        CropThreadBlock *crop = cropBlocks[i] = new CropThreadBlock(i);
        crop->ConnectInput(c2->NewOutputPin());
        crop->ConnectOutput(c3->NewInputPin());
        crop->SetOutResolution(224, 224);
        crop->SetOutDump(dump_crop);
        crop->Prepare();
    }
    printf("After crop prepare\n");

    for (int i = 0; i < classification_num; i++)
    {
        InferenceThreadBlock *cla = classBlocks[i] = new InferenceThreadBlock(i, RESNET_50);
        VACsvWriterPin *sink = new VACsvWriterPin("outputs.csv");
        cla->ConnectInput(c3->NewOutputPin());
        cla->ConnectOutput(sink);
        cla->SetAsyncDepth(1);
        cla->SetBatchNum(1);
        cla->SetDevice("GPU");
        cla->SetModelFile("../../models/resnet-50-128.xml", "../../models/resnet-50-128.bin");
        cla->Prepare();
    }
    printf("After classification prepare\n");

    VAThreadBlock::RunAllThreads();

    Statistics::getInstance().ReportPeriodly(1.0, duration);

    VAThreadBlock::StopAllThreads();

    for (int i = 0; i < channel_num; i++)
    {
        delete decodeBlocks[i];
    }
    
    for (int i = 0; i < inference_num; i++)
    {
        delete inferBlocks[i];
    }
    for (int i = 0; i < crop_num; i++)
    {
        delete cropBlocks[i];
    }

    for (int i = 0; i < classification_num; i++)
    {
        delete classBlocks[i];
    }
    return 0;
}
