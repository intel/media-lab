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

#include "Statistics.h"
#include <unistd.h>
#include <signal.h>
#include <math.h>

static bool _continue = true;
// handle to end the process
void sigint_hanlder(int s)
{
    _continue = false;
}

Statistics::Statistics()
{
    for (int i = 0; i < STATISTICS_TYPE_NUM; i++)
    {
        m_counters[i] = 0;
        pthread_mutex_init(&m_mutex[i], nullptr);
    }
    // set the handler of ctrl-c
    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = sigint_hanlder;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);
}

Statistics::~Statistics()
{
    for (int i = 0; i < STATISTICS_TYPE_NUM; i++)
    {
        pthread_mutex_destroy(&m_mutex[i]);
    }
}

void Statistics::Step(StatisticsType type)
{
    pthread_mutex_lock(&m_mutex[type]);
    ++ m_counters[type];
    pthread_mutex_unlock(&m_mutex[type]);
}

void Statistics::Clear(StatisticsType type)
{
    
    if (type == STATISTICS_TYPE_NUM)
    {
        for (int i = 0; i < STATISTICS_TYPE_NUM; i++)
        {
            pthread_mutex_lock(&m_mutex[i]);
            m_counters[i] = 0;
            pthread_mutex_unlock(&m_mutex[i]);
        }
    }
    else
    {
        pthread_mutex_lock(&m_mutex[type]);
        m_counters[type] = 0;
        pthread_mutex_unlock(&m_mutex[type]);
    }
    
}

void Statistics::Report()
{
    printf("Decode FPS: %d\n", m_counters[DECODED_FRAMES]);
}

void Statistics::ReportPeriodly(float period, int duration)
{
    int count = 0;
    bool endless = (duration < 0);
    while (_continue)
    {
        uint32_t time = (uint32_t)(period * 1000000);
        usleep(time);
        Report();
        Clear();
        ++ count;
        if (!endless && count >= (int)ceil(duration / period))
        {
            break;
        }
    }
}