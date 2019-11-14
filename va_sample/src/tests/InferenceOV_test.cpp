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

#include <unistd.h>
#include <iostream>
#include <string>
#include "Inference.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/opencv.hpp>


#include "DataPacket.h"

const char *device = "GPU";

const char *image_file1[] = {
                            "../../clips/0.jpg",
                            "../../clips/1.jpg",
                            "../../clips/2.jpg",
                            "../../clips/3.jpg",
                            "../../clips/4.jpg"
                            };
const char *image_file2[] = {
                            "../../clips/5.jpg",
                            "../../clips/6.jpg",
                            "../../clips/7.jpg",
                            "../../clips/8.jpg",
                            "../../clips/9.jpg",
                            };

int InsertRGBFrame(InferenceBlock *infer, const cv::Mat &image, uint32_t channel, uint32_t frame)
{
    // The img in BGR format
    std::vector<cv::Mat> inputChannels;
    int channelNum = 3;
    int width = image.size[0];
    int height = image.size[1];

    uint8_t *input = new uint8_t[width * height * 3];
    for (int i = 0; i < channelNum; i ++)
    {
        cv::Mat channel(height, width, CV_8UC1, input + i * width * height);
        inputChannels.push_back(channel);
    }
    cv::split(image, inputChannels);

    infer->InsertImage(input, channel, frame);

    delete[] input;
    return 0;
}

int TestSSD()
{
    int ret = 0;
    InferenceBlock *infer = InferenceBlock::Create(MOBILENET_SSD_U8);
    infer->Initialize(5, 3);
    ret = infer->Load(device, "../../models/mobilenet-ssd.xml", "../../models/mobilenet-ssd.bin");
    if (ret)
    {
        std::cout << "load model failed!\n";
        return -1;
    }

    std::vector<cv::Mat> frames1;
    for (int i = 0; i < 5; i++)
    {
        cv::Mat src, frame;

        src = cv::imread(image_file1[i], cv::IMREAD_COLOR);
        resize(src, frame, {300, 300});
        frames1.push_back(frame);
    }
    std::vector<cv::Mat> frames2;
    for (int i = 0; i < 5; i++)
    {
        cv::Mat src, frame;

        src = cv::imread(image_file2[i], cv::IMREAD_COLOR);
        resize(src, frame, {300, 300});
        frames2.push_back(frame);
    }

    InsertRGBFrame(infer, frames1[0], 0, 0);
    InsertRGBFrame(infer, frames2[0], 1, 0);
    InsertRGBFrame(infer, frames1[1], 0, 1);
    InsertRGBFrame(infer, frames1[2], 0, 2);
    InsertRGBFrame(infer, frames2[1], 1, 1);
    InsertRGBFrame(infer, frames2[2], 1, 2);
    InsertRGBFrame(infer, frames2[3], 1, 3);
    InsertRGBFrame(infer, frames1[3], 0, 3);
    InsertRGBFrame(infer, frames1[4], 0, 4);
    InsertRGBFrame(infer, frames2[4], 1, 4);
    std::vector<VAData *> outputs;
    std::vector<uint32_t> channels;
    std::vector<uint32_t> frames;
    while(1)
    {
        ret = infer->GetOutput(outputs, channels, frames);
        //printf("%d outputs\n", outputs.size());
        if (ret == -1)
        {
            break;
        }
    }

    for (int i = 0; i < outputs.size(); i++)
    {
        int channel, frame;
        //printf("channel %d, frame %d\n", outputs[i]->ChannelIndex(), outputs[i]->FrameIndex());
    }
    
    cv::Mat screen = cv::Mat(300, 300, CV_8UC3);

    int index = 0;
    for (int i = 0; i < channels.size(); i++)
    {
        //printf("channel %d, frame %d\n", channels[i], frames[i]);

        if (channels[i] == 0)
        {
            frames1[frames[i]].copyTo(screen);
        }
        else if (channels[i] == 1)
        {
            frames2[frames[i]].copyTo(screen);
        }

        while (index < outputs.size()
            && outputs[index]->ChannelIndex() == channels[i]
            && outputs[index]->FrameIndex() == frames[i])
        {
            float left, top, right, bottom;
            outputs[index]->GetRoiRegion(&left, &top, &right, &bottom);
            uint32_t l, t, r, b;
            l = (uint32_t)(left * 300);
            t = (uint32_t)(top * 300);
            r = (uint32_t)(right * 300);
            b = (uint32_t)(bottom * 300);
            cv::rectangle(screen, cv::Point(l, t), cv::Point(r, b), cv::Scalar(71, 99, 250), 2);
            ++ index;
        }

        cv::namedWindow( "Display window", cv::WINDOW_AUTOSIZE );
        cv::imshow( "Display window", screen );
        cv::waitKey();
    }

    InferenceBlock::Destroy(infer);

    return 0;
}

int TestResnet()
{
    int ret = 0;
    InferenceBlock *infer = InferenceBlock::Create(RESNET_50);
    infer->Initialize(5, 3);
    ret = infer->Load(device, "../../models/resnet-50-128.xml", "../../models/resnet-50-128.bin");
    if (ret)
    {
        std::cout << "load model failed!\n";
        return -1;
    }

    std::vector<cv::Mat> frames1;
    for (int i = 0; i < 5; i++)
    {
        cv::Mat src, frame;

        src = cv::imread("../../clips/cat.jpg", cv::IMREAD_COLOR);
        resize(src, frame, {224, 224});
        frames1.push_back(frame);
    }
    
    std::vector<cv::Mat> frames2;
    for (int i = 0; i < 5; i++)
    {
        cv::Mat src, frame;

        src = cv::imread("../../clips/car.jpg", cv::IMREAD_COLOR);
        resize(src, frame, {224, 224});
        frames2.push_back(frame);
    }

    InsertRGBFrame(infer, frames1[0], 0, 0);
    InsertRGBFrame(infer, frames2[0], 1, 0);
    InsertRGBFrame(infer, frames1[1], 0, 1);
    InsertRGBFrame(infer, frames1[2], 0, 2);
    InsertRGBFrame(infer, frames2[1], 1, 1);
    InsertRGBFrame(infer, frames2[2], 1, 2);
    InsertRGBFrame(infer, frames2[3], 1, 3);
    InsertRGBFrame(infer, frames1[3], 0, 3);
    InsertRGBFrame(infer, frames1[4], 0, 4);
    InsertRGBFrame(infer, frames2[4], 1, 4);
    
    std::vector<VAData *> outputs;
    std::vector<uint32_t> channels;
    std::vector<uint32_t> frames;
    while(1)
    {
        ret = infer->GetOutput(outputs, channels, frames);
        //printf("%d outputs\n", outputs.size());
        if (ret == -1)
        {
            break;
        }
    }

    cv::Mat screen = cv::Mat(224, 224, CV_8UC3);

    int index = 0;
    for (int i = 0; i < channels.size(); i++)
    {
        //printf("channel %d, frame %d\n", channels[i], frames[i]);

        if (channels[i] == 0)
        {
            frames1[frames[i]].copyTo(screen);
        }
        else if (channels[i] == 1)
        {
            frames2[frames[i]].copyTo(screen);
        }

        printf("channel %d, frame %d, class %d, conf %f\n", outputs[i]->ChannelIndex(),
                                                            outputs[i]->FrameIndex(),
                                                            outputs[i]->Class(),
                                                            outputs[i]->Confidence());
        cv::namedWindow( "Display window", cv::WINDOW_AUTOSIZE );
        cv::imshow( "Display window", screen );
        cv::waitKey();
    }
    InferenceBlock::Destroy(infer);

    return 0;
}

int main()
{
    TestSSD();
    TestResnet();
}
