#include <unistd.h>
#include <iostream>
#include <string>
#include "InferenceMobileSSD.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>

#include "DataPacket.h"

const char *device = "GPU";
const char *model_xml = "../../models/mobilenet-ssd.xml";
const char *model_bin = "../../models/mobilenet-ssd.bin";
//const char *model_xml = "../../models/MobileNetSSD_deploy_32.xml";
//const char *model_bin = "../../models/MobileNetSSD_deploy_32.bin";

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

int TestSSD()
{
    int ret = 0;
    InferenceMobileSSD infer;
    infer.Initialize(5, 3);
    ret = infer.Load(device, model_xml, model_bin);
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

    infer.InsertImage(frames1[0], 0, 0);
    infer.InsertImage(frames2[0], 1, 0);
    infer.InsertImage(frames1[1], 0, 1);
    infer.InsertImage(frames1[2], 0, 2);
    infer.InsertImage(frames2[1], 1, 1);
    infer.InsertImage(frames2[2], 1, 2);
    infer.InsertImage(frames2[3], 1, 3);
    infer.InsertImage(frames1[3], 0, 3);
    infer.InsertImage(frames1[4], 0, 4);
    infer.InsertImage(frames2[4], 1, 4);
    std::vector<VAData *> outputs;
    std::vector<uint32_t> channels;
    std::vector<uint32_t> frames;
    while(1)
    {
        ret = infer.GetOutput(outputs, channels, frames);
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
            uint32_t x, y, w, h;
            outputs[index]->GetRoiRegion(&x, &y, &w, &h);
            cv::rectangle(screen, cv::Point(x, y), cv::Point(x+w, y+h), cv::Scalar(71, 99, 250), 2);
            ++ index;
        }

        cv::namedWindow( "Display window", cv::WINDOW_AUTOSIZE );
        cv::imshow( "Display window", screen );
        cv::waitKey();
    }

    return 0;
}

int main()
{
    TestSSD();
}
