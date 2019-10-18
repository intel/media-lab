/*
// Copyright (c) 2019 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/
#include "Statistics.h"
#include <unistd.h>

Statistics::Statistics()
{
    for (int i = 0; i < STATISTICS_TYPE_NUM; i++)
    {
        m_counters[i] = 0;
        pthread_mutex_init(&m_mutex[i], nullptr);
    }
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

void Statistics::ReportPeriodly(float second)
{
    while (true)
    {
        uint32_t time = (uint32_t)(second * 1000000);
        usleep(time);
        Report();
        Clear();
    }
    
}