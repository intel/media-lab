#pragma once

#include <stdio.h>
#include <unistd.h>
#include <string>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/opencv.hpp>

#include "ThreadBlock.h"

class DisplayThreadBlock : public VAThreadBlock
{
public:
    DisplayThreadBlock()
    {
    }

    ~DisplayThreadBlock()
    {
    }

    int Prepare();

    int Loop();

protected:
    bool IsVpOut(VAData *data);
    bool IsRoi(VAData *data);
    cv::Mat m_screen;

private:
};