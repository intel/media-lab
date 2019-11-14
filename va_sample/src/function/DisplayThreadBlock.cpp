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

#include "DisplayThreadBlock.h"

int DisplayThreadBlock::PrepareInternal()
{
    m_screen = cv::Mat(300, 300, CV_8UC3);

    return 0;
}

int DisplayThreadBlock::Loop()
{
    while (true)
    {
        VADataPacket *InPacket = AcquireInput();
        VADataPacket *OutPacket = DequeueOutput();

        VAData *vpOut = nullptr;
        std::vector<VAData *>rois;

        for (auto ite = InPacket->begin(); ite != InPacket->end(); ite++)
        {
            VAData *data = *ite;
            if (IsVpOut(data))
            {
                vpOut = data;
            }
            else if (IsRoi(data))
            {
                rois.push_back(data);
            }
            else
            {
                OutPacket->push_back(data);
            }
        }

        ReleaseInput(InPacket);

        if (!vpOut)
        {
            printf("No VPOut\n");
            continue;
        }

        cv::Mat frame;
        
        uint32_t w, h, p, fourcc;
        vpOut->GetSurfaceInfo(&w, &h, &p, &fourcc);
        uint8_t *data = vpOut->GetSurfacePointer();
        std::vector<cv::Mat> channels;
        cv::Mat R = cv::Mat(w, h, CV_8UC1, data);
        cv::Mat G = cv::Mat(w, h, CV_8UC1, data+w*h);
        cv::Mat B = cv::Mat(w, h, CV_8UC1, data+w*h*2);
        channels.push_back(B);
        channels.push_back(G);
        channels.push_back(R);
        cv::merge(channels, frame);
        
        for (int i = 0; i < rois.size(); i++)
        {
            float left, top, right, bottom;
            rois[i]->GetRoiRegion(&left, &top, &right, &bottom); 
            uint32_t l, t, r, b;
            l = (uint32_t)(left * w);
            t = (uint32_t)(top * h);
            r = (uint32_t)(right * w);
            b = (uint32_t)(bottom * h);

            //printf("display, %d, %d, %d\n", rois[i]->ChannelIndex(), rois[i]->FrameIndex(), rois[i]->RoiIndex());
            cv::rectangle(frame, cv::Point(l, t), cv::Point(r, b), cv::Scalar(71, 99, 250), 2);
            rois[i]->DeRef(OutPacket);
        }
        frame.copyTo(m_screen);
        vpOut->DeRef(OutPacket);

        EnqueueOutput(OutPacket);
        
        cv::namedWindow( "Display window", cv::WINDOW_AUTOSIZE );
        cv::imshow( "Display window", m_screen );
        cv::waitKey();
    }
    return 0;
}

bool DisplayThreadBlock::IsVpOut(VAData *data)
{
    uint32_t w, h, p, fourcc;
    data->GetSurfaceInfo(&w, &h, &p, &fourcc);
    return (w == 300) && (h == 300) && (fourcc == MFX_FOURCC_RGBP);
}

bool DisplayThreadBlock::IsRoi(VAData *data)
{
    return data->Type() == ROI_REGION;
}

