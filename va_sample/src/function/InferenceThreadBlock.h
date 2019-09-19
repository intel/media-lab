#pragma once

#include <stdio.h>
#include <unistd.h>
#include <string>

#include "ThreadBlock.h"
#include "../../libs/detector.h"

class InferenceThreadBlock : public VAThreadBlock
{
public:
    InferenceThreadBlock(int channel);

    ~InferenceThreadBlock();

    int Prepare();

    int Loop();

protected:
    bool CanBeProcessed(VAData *data)
    {
        uint32_t w, h, p, fourcc;
        data->GetSurfaceInfo(&w, &h, &p, &fourcc);
        return (w == 300) && (h == 300) && (fourcc == MFX_FOURCC_RGBP);
    }
    uint32_t m_index;

private:
    Detector d;
    std::string device = "GPU";
    const std::string model_xml = "/home/hefan/workspace/VA/video_analytics_Intel_GPU/test_content/IR/SSD_mobilenet/MobileNetSSD_deploy_32.xml";
    const std::string model_bin = "/home/hefan/workspace/VA/video_analytics_Intel_GPU/test_content/IR/SSD_mobilenet/MobileNetSSD_deploy_32.bin";
};