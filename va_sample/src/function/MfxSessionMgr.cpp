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
#include "MfxSessionMgr.h"
#include <unistd.h>
#include "common.h"

MfxSessionMgr::MfxSessionMgr()
{
}

MfxSessionMgr::~MfxSessionMgr()
{
    for (auto ite = m_mfxSessions.begin(); ite != m_mfxSessions.end(); ite ++)
    {
        if (ite->second)
        {
            delete ite->second;
        }
    }
    for (auto ite = m_mfxAllocators.begin(); ite != m_mfxAllocators.end(); ite ++)
    {
        if (ite->second)
        {
            delete ite->second;
        }
    }
    m_mfxSessions.clear();
    m_mfxAllocators.clear();
}

void MfxSessionMgr::Clear(uint32_t channel)
{
    auto ite = m_mfxSessions.find(channel);
    if (ite != m_mfxSessions.end())
    {
        delete ite->second;
        m_mfxSessions.erase(ite);
    }
    auto ite2 = m_mfxAllocators.find(channel);
    if (ite2 != m_mfxAllocators.end())
    {
        delete ite2->second;
        m_mfxAllocators.erase(ite2);
    }
}

MFXVideoSession *MfxSessionMgr::GetSession(uint32_t channel)
{
    auto ite = m_mfxSessions.find(channel);
    if (ite == m_mfxSessions.end())
    {
        if (NewChannel(channel) != MFX_ERR_NONE)
        {
            return nullptr;
        }
    }
    return m_mfxSessions[channel];
}

mfxFrameAllocator *MfxSessionMgr::GetAllocator(uint32_t channel)
{
    auto ite = m_mfxAllocators.find(channel);
    if (ite == m_mfxAllocators.end())
    {
        if (NewChannel(channel) != MFX_ERR_NONE)
        {
            return nullptr;
        }
    }
    return m_mfxAllocators[channel];
}

int MfxSessionMgr::NewChannel(uint32_t channel)
{
    MFXVideoSession *session = new MFXVideoSession();
    mfxFrameAllocator *allocator = new mfxFrameAllocator();

    mfxStatus sts = MFX_ERR_NONE;
    mfxIMPL impl = MFX_IMPL_AUTO_ANY; // SDK implementation type: hardware accelerator?, software? or else
    mfxVersion ver = { {0, 1} }; // media sdk version

    sts = Initialize(impl, ver, session, allocator);
    if (sts != MFX_ERR_NONE)
    {
        delete session;
        delete allocator;
        return sts;
    }

    m_mfxSessions[channel] = session;
    m_mfxAllocators[channel] = allocator;

    return MFX_ERR_NONE;
}
