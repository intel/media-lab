/*
// Copyright (c) 2018 Intel Corporation
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

/*
// brief The entry point for the Inference Engine object_detection sample application
*/

#ifndef _DUALPIPE_H_
#define _DUALPIPE_H_

#include <pthread.h>
#include <list>
#include <stdio.h>

class VaDualPipe
{
typedef void (*bufferInitFunc)(void *);
public:
    VaDualPipe();
    ~VaDualPipe();
    int Initialize(int nframe, int maxframesize, bufferInitFunc init = NULL);
    void *Get();
    void Put(void *buffer);
    void *Load(const timespec *abstime);
    void *LoadNoWait();
    void Store(void *buffer);

protected:
    std::list<void *> m_inPipe;
    std::list<void *> m_outPipe;

    pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
};

#endif
