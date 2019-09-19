#include <stdio.h>
#include <string>

#include "DataPacket.h"
#include "ConnectorRR.h"
#include "InferenceThreadBlock.h"
#include "DecodeThreadBlock.h"

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
        InferenceThreadBlock *infer = inferBlocks[i] = new InferenceThreadBlock(i);
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

        dec->Prepare();
        infer->Prepare();
    }

    for (int i = 0; i < channel_num; i++)
    {
        decodeBlocks[i]->Run();
        inferBlocks[i]->Run();
    }

    pause();

    return 0;
}