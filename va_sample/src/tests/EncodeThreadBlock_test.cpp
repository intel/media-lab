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
#include "common.h"
#include "DecodeThreadBlock.h"
#include "EncodeThreadBlock.h"
#include "ConnectorRR.h"
#include <mfxvideo++.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <string>
#include <sstream>
#include <memory>

static int vp_ratio = 1;
static int channel_num = 1;
static int sfc_switch = 0;
static int vpp_w = 300;
static int vpp_h = 300;
std::string input_filename;
static int vpp_output_fourcc = MFX_FOURCC_NV12;
static bool dump_vp = false;

void App_ShowUsage(void)
{
    printf("Usage: multichannel_decode -i input.264 [-r vp_ratio] [-c channel_number] [-d]\n");
    printf("           -r vp_ratio: the ratio of decoded frames to vp frames, e.g., -r 2 means doing vp every other frame\n");
    printf("                        5 by default\n");
    printf("                        0 means no vp\n");
    printf("           -c channel_number: number of decoding channels, default value is 1\n");
    printf("           -d dump the VP output, default value is off\n");
    printf("           -sfc_switch: values 0,1,2. 0 - disable sfc, 1 - enable sfc. 2 - enable sfc for half streams\n");
    printf("           -w_vpp <val> -h_vpp <val>. Target weight & height for VPP resize\n");
    printf("           -vpp_output_format. Output format for VPP: nv12, rgbp, rgb4 \n");
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
        if (sources.at(i) == "-c")
            channel_num = stoi(sources.at(++i));
        else if (sources.at(i) == "-i")
            input_filename = sources.at(++i);
        else if (sources.at(i) == "-r")
            vp_ratio = stoi(sources.at(++i));
        else if (sources.at(i) == "-d")
            dump_vp = true;
        else if (sources.at(i) == "-w_vpp")
            vpp_w = stoi(sources.at(++i));
        else if (sources.at(i) == "-h_vpp")
            vpp_h = stoi(sources.at(++i));
        else if (sources.at(i) == "-sfc_switch")
            sfc_switch = stoi(sources.at(++i));
        else if (sources.at(i) == "-vpp_output_format")
        {
            std::string format = sources.at(++i);
            if (format == "nv12")
                vpp_output_fourcc = MFX_FOURCC_NV12;
            else if (format == "rgbp")
                vpp_output_fourcc = MFX_FOURCC_RGBP;
            else if (format == "rgb4")
                vpp_output_fourcc = MFX_FOURCC_RGB4;
        }
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

    mfxStatus sts = MFX_ERR_NONE;
    mfxIMPL impl = MFX_IMPL_AUTO_ANY; // SDK implementation type: hardware accelerator?, software? or else
    mfxVersion ver = { {0, 1} };      // media sdk version

    DecodeThreadBlock **decodeBlocks = new DecodeThreadBlock *[channel_num];
    EncodeThreadBlock **encodeBlocks = new EncodeThreadBlock *[channel_num];
    VAConnectorRR **connectors = new VAConnectorRR *[channel_num];
    VAFilePin **filePins = new VAFilePin *[channel_num];
    VASinkPin **sinks = new VASinkPin *[channel_num];

    mfxFrameAllocator allocator;
    std::vector<std::unique_ptr<MFXVideoSession>> mfxSessionHolder;
    mfxSessionHolder.reserve(64);

    for (int i = 0; i < channel_num; i++)
    {
        mfxSessionHolder.push_back(std::unique_ptr<MFXVideoSession> (new MFXVideoSession));
        sts = Initialize(impl, ver, (MFXVideoSession *)mfxSessionHolder.at(i).get(), &allocator);
        if( sts != MFX_ERR_NONE)
        {
            printf("\t. Failed to initialize MSDk session \n");
        }

        DecodeThreadBlock *d = decodeBlocks[i] = new DecodeThreadBlock(i, mfxSessionHolder.at(i).get(), &allocator);
        EncodeThreadBlock *e = encodeBlocks[i] = new EncodeThreadBlock(i, mfxSessionHolder.at(i).get(), &allocator);
        VAFilePin *pin = filePins[i] = new VAFilePin(input_filename.c_str());
        VASinkPin *sink = sinks[i] = new VASinkPin();
        VAConnectorRR *c1 = connectors[i] = new VAConnectorRR(1, 1, 10);

        d->ConnectInput(pin);
        d->ConnectOutput(c1->NewInputPin());

        e->ConnectInput(c1->NewOutputPin());
        e->ConnectOutput(sink);

        d->SetDecodeOutputRef(0);
        d->SetVPOutputRef(0);
        d->SetVPMemOutTypeVideo(true);
        if (0 == sfc_switch)  // SFC resize off
            d->SetDecPostProc(false);
        else if (1 == sfc_switch) // SFC resize on
            d->SetDecPostProc(true);
        else if (2 == sfc_switch) // SFC resize on for half streams
        {
            if (1 == (i%2))
                d->SetDecPostProc(true);
        }

        d->SetVPRatio(vp_ratio);

        d->SetVPOutFormat(vpp_output_fourcc);

        d->SetVPOutResolution(vpp_w, vpp_h);

        d->SetVPOutDump(dump_vp);

        d->Prepare();

        e->SetAsyncDepth(1);
        e->SetInputFormat(vpp_output_fourcc);
        e->SetInputResolution(vpp_w, vpp_h);
        e->SetEncodeOutDump(true);
        e->Prepare();
    }

    for (int i = 0; i < channel_num; i++)
    {
        decodeBlocks[i]->Run();
        encodeBlocks[i]->Run();
    }

    pause();
}




