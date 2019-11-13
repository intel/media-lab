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
