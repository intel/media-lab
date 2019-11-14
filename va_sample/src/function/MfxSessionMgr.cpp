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
