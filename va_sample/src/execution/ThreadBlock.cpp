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

#include "ThreadBlock.h"

std::vector<VAThreadBlock *> VAThreadBlock::m_allThreads;

static void *VAThreadFunc(void *arg)
{
    VAThreadBlock *block = static_cast<VAThreadBlock *>(arg);
    block->Loop();
    return (void *)0;
}

VAThreadBlock::VAThreadBlock():
    m_continue(false),
    m_inputPin(nullptr),
    m_outputPin(nullptr)
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
    pthread_cancel(m_threadId);
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

