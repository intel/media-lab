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

#include <iostream>
#include <string>
#include "detector.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>

std::string device = "GPU";
const std::string model_xml = "/home/fresh/data/model/r2/mobilenet-ssd.xml";
const std::string model_bin = "/home/fresh/data/model/r2/mobilenet-ssd.bin";
const std::string image_file = "/home/fresh/1.jpg";

int main()
{
    int ret = 0;
    Detector d;

    ret = d.Load(device, model_xml, model_bin, 1);
    if (ret)
    {
        std::cout << "load model failed!\n";
        return -1;
    }

    cv::Size net_size = d.GetNetSize();
    int input_width = net_size.width;
    int input_height = net_size.height;
    Detector::InsertImgStatus status;
    vector<Detector::DetctorResult> objects;
    cv::Mat src, frame;

    src = cv::imread(image_file, cv::IMREAD_COLOR);
    resize(src, frame, {300, 300});

    status = d.InsertImage(frame, objects, 0, 0);

    if (Detector::INSERTIMG_GET == status || Detector::INSERTIMG_PROCESSED == status)
    {
        std::cout << " Infer:"
                  << " done one frame : objects = " << objects.size() << std::endl;

        for (int k = 0; k < objects.size(); k++)
        {
            std::cout << " detect result: " << objects[k].channelid << " boxes = " << objects[k].boxs.size() << std::endl;
            for (auto b : objects[k].boxs)
            {
                std::cout << "ClassID = " << b.classid << ", Confidence = " << b.confidence << ", [" << b.left << ", " << b.top << ", " << b.right << ", " << b.bottom << "]\n";
            }
        }
    }

    std::cout << "done.\n";
    return 0;
}
