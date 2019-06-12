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

#include "dualpipe.h"
#include <malloc.h>
#include <unistd.h>

VaDualPipe::VaDualPipe()
{
    pthread_mutex_init(&m_mutex, NULL);
    pthread_cond_init(&m_cond, NULL);
}

int VaDualPipe::Initialize(int nframe, int maxframesize, bufferInitFunc init)
{
    for (int i = 0; i < nframe; i ++)
    {
        void *data = memalign(16, maxframesize);
        if (data == NULL)
        {
            fprintf(stderr, "Error in buffer allocation\n");
            return -1;
        }
        if (init != NULL)
        {
            init(data);
        }
        m_inPipe.push_front(data);
    }
    return 0;
}

VaDualPipe::~VaDualPipe()
{
    while(m_inPipe.size() > 0)
    {
        void *data = m_inPipe.front();
        free(data);
        m_inPipe.pop_front();
    }

    while(m_outPipe.size() > 0)
    {
        void *data = m_outPipe.front();
        free(data);
        m_outPipe.pop_front();
    }
    pthread_mutex_destroy(&m_mutex);
    pthread_cond_destroy(&m_cond);
}

void *VaDualPipe::Get()
{
    void *buffer = NULL;
    do
    {
        pthread_mutex_lock(&m_mutex);
        if (m_inPipe.size() > 0)
        {
            buffer = m_inPipe.front();
            m_inPipe.pop_front();
        }
        else
        {
            pthread_mutex_unlock(&m_mutex);
            usleep(500);
        }
    } while (buffer == NULL);
    pthread_mutex_unlock(&m_mutex);
    return buffer;
}

void VaDualPipe::Put(void *buffer)
{
    pthread_mutex_lock(&m_mutex);
    m_inPipe.push_front(buffer);
    pthread_mutex_unlock(&m_mutex);
}

void *VaDualPipe::Load(const timespec *abstime)
{
    bool timeout = false;
    void *buffer = NULL;
    pthread_mutex_lock(&m_mutex);
    do
    {
        if (m_outPipe.size() > 0)
        {
            buffer = m_outPipe.front();
            m_outPipe.pop_front();
        }
        else if (abstime == NULL)
        {
            pthread_cond_wait(&m_cond, &m_mutex);
        }
        else if (!timeout)
        {
            pthread_cond_timedwait(&m_cond, &m_mutex, abstime);
            timeout = true;
        }
        else if (timeout)
        {
            break;
        }
    } while(buffer == NULL);
    pthread_mutex_unlock(&m_mutex);

    return buffer;
}

void *VaDualPipe::LoadNoWait()
{
    void *buffer = NULL;
    pthread_mutex_lock(&m_mutex);
    if (m_outPipe.size() > 0)
    {
        buffer = m_outPipe.front();
        m_outPipe.pop_front();
    }
    pthread_mutex_unlock(&m_mutex);

    return buffer;
}

void VaDualPipe::Store(void *buffer)
{
    pthread_mutex_lock(&m_mutex);
    m_outPipe.push_back(buffer);
    pthread_mutex_unlock(&m_mutex);
    pthread_cond_signal(&m_cond);
}

