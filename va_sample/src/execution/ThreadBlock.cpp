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

#include "ThreadBlock.h"
#include <unistd.h>

std::vector<VAThreadBlock *> VAThreadBlock::m_allThreads;

static void *VAThreadFunc(void *arg)
{
    VAThreadBlock *block = static_cast<VAThreadBlock *>(arg);
    block->Loop();
    block->Finish();
    return (void *)0;
}

VAThreadBlock::VAThreadBlock():
    m_inputPin(nullptr),
    m_outputPin(nullptr),
    m_stop(false),
    m_finish(false)
{
    
}

VAThreadBlock::~VAThreadBlock()
{
}

int VAThreadBlock::Prepare()
{
    m_allThreads.push_back(this);
    return PrepareInternal();
}

int VAThreadBlock::Run()
{
    return pthread_create(&m_threadId, nullptr, VAThreadFunc, (void *)this);
}

int VAThreadBlock::Stop()
{
    m_stop = true;
    for (int i = 0; i < 1000; i ++)
    {
        if (m_finish)
        {
            break;
        }
        usleep(1000);
    }
    if (!m_finish)
    {
        pthread_cancel(m_threadId);
    }
    pthread_join(m_threadId, nullptr);

}

void VAThreadBlock::RunAllThreads()
{
    for (auto ite = m_allThreads.begin(); ite != m_allThreads.end(); ite ++)
    {
        VAThreadBlock *t = *ite;
        t->Run();
    }
}

void VAThreadBlock::StopAllThreads()
{
    for (auto ite = m_allThreads.begin(); ite != m_allThreads.end(); ite ++)
    {
        VAThreadBlock *t = *ite;
        t->Stop();
    }
}

