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

#ifndef __MFX_SESSION_MGR_H__
#define __MFX_SESSION_MGR_H__
#include <stdint.h>
#include <pthread.h>
#include <stdio.h>
#include <mfxvideo++.h>
#include <mfxstructures.h>
#include <map>

class MfxSessionMgr
{
public:
    static MfxSessionMgr& getInstance()
    {
        static MfxSessionMgr instance;
        return instance;
    }
    ~MfxSessionMgr();

    MFXVideoSession *GetSession(uint32_t channel);
    mfxFrameAllocator *GetAllocator(uint32_t channel);

private:
    MfxSessionMgr();
    int NewChannel(uint32_t channel);

    std::map<uint32_t, MFXVideoSession*> m_mfxSessions;
    std::map<uint32_t, mfxFrameAllocator*> m_mfxAllocators;
};


#endif
