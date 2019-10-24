#include <stdio.h>
#include <string>

#include "DataPacket.h"
#include "ConnectorRR.h"
#include "CropThreadBlock.h"
#include "DecodeThreadBlock.h"
#include "InferenceThreadBlock.h"
#include "Statistics.h"


const std::string input_file = "/home/hefan/workspace/VA/clips/2m.264";
const std::string input_file1 = "/home/hefan/workspace/VA/video_analytics_Intel_GPU/test_content/video/1.h264";

int main(int argc, char *argv[])
{
    int channel_num = 1;
    int inference_num = 1;
    int crop_num = 1;
    int classification_num = 1;
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
        if (i == 0)
            pin = filePins[i] = new VAFilePin(input_file.c_str());
        else
            pin = filePins[i] = new VAFilePin(input_file1.c_str());
        dec->ConnectInput(pin);
        dec->ConnectOutput(c1->NewInputPin());
        dec->SetDecodeOutputRef(0);
        dec->SetVPOutputRef(1);
        dec->SetDecodeOutputWithVP(); // when there is vp output, decode output also attached
        dec->SetVPRatio(5);
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
        //crop->SetOutDump();
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

    for (int i = 0; i < channel_num; i++)
    {
        decodeBlocks[i]->Run();
    }

    for (int i = 0; i < inference_num; i++)
    {
        printf("Run inference\n");
        inferBlocks[i]->Run();
    }

    for (int i = 0; i < crop_num; i++)
    {
        cropBlocks[i]->Run();
    }

    for (int i = 0; i < classification_num; i++)
    {
        classBlocks[i]->Run();
    }
    Statistics::getInstance().ReportPeriodly(1.0);

    pause();

    return 0;
}
