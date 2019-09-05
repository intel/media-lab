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
#include "DecodeThreadBlock.h"
#include <mfxvideo++.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <sstream>

static int vp_ratio = 1;
static int channel_num = 1;
std::string input_filename;
static bool dump_vp = false;

void App_ShowUsage(void)
{
    printf("Usage: multichannel_decode -i input.264 [-r vp_ratio] [-c channel_number] [-d]\n");
    printf("           -r vp_ratio: the ratio of decoded frames to vp frames, e.g., -r 2 means doing vp every other frame\n");
    printf("                        5 by default\n");
    printf("                        0 means no vp\n");
    printf("           -c channel_number: number of decoding channels, default value is 1\n");
    printf("           -d dump the VP output, default value is off\n");
}

int ParseOpt(int argc, char *argv[])
{
    int c;
    while ((c = getopt (argc, argv, "c:i:r:d")) != -1)
    {
    switch (c)
      {
      case 'c':
        channel_num = atoi(optarg);
        break;
      case 'i':
        input_filename = optarg;
        break;
      case 'r':
        vp_ratio = atoi(optarg);
        break;
      case 'd':
        dump_vp = true;
        break;
      case 'h':
        App_ShowUsage();
        exit(0);
      default:
        abort ();
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

    DecodeThreadBlock **decodeBlocks = new DecodeThreadBlock *[channel_num];
    VAFilePin **filePins = new VAFilePin *[channel_num];
    VASinkPin **sinks = new VASinkPin *[channel_num];

    for (int i = 0; i < channel_num; i++)
    {
        DecodeThreadBlock *t = decodeBlocks[i] = new DecodeThreadBlock(i);
        VAFilePin *pin = filePins[i] = new VAFilePin(input_filename.c_str());
        VASinkPin *sink = sinks[i] = new VASinkPin();

        t->ConnectInput(pin);
        t->ConnectOutput(sink);

        t->SetDecodeOutputRef(0);

        t->SetVPRatio(vp_ratio);

        t->SetVPOutResolution(300, 300);

        //t->SetVPOutDump();

        t->Prepare();
    }

    for (int i = 0; i < channel_num; i++)
    {
        decodeBlocks[i]->Run();
    }
    pause();
}

