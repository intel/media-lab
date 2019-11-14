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

#ifndef __STATISTICS_H__
#define __STATISTICS_H__
#include <stdint.h>
#include <pthread.h>
#include <stdio.h>

enum StatisticsType
{
    DECODED_FRAMES = 0,
    STATISTICS_TYPE_NUM
};

class Statistics
{
public:
    static Statistics& getInstance()
    {
        static Statistics instance;
        return instance;
    }
    ~Statistics();

    void Step(StatisticsType type);

    void Clear(StatisticsType type = STATISTICS_TYPE_NUM);

    void Report();

    void ReportPeriodly(float period, int duration = -1);

private:
    Statistics();

    pthread_mutex_t m_mutex[STATISTICS_TYPE_NUM];
    uint32_t m_counters[STATISTICS_TYPE_NUM];
};


#endif
