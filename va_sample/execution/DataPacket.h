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

#ifndef __DATA_PACKET_H__
#define __DATA_PACKET_H__
#include <stdint.h>
#include <pthread.h>
#include <stdio.h>

enum VA_DATA_TYPE
{
    USER_SURFACE,
    MFX_SURFACE,
    OCV_SURFACE,
    ROI_REGION,
    USER_BUFFER,
};

struct UserSurface
{
    int mem_type;
    int format;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    void *data
};

MfxSurface;

struct RoiRegion
{
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
};

class VAData
{
public:
    VAData(MfxSurface* s);
    VAData(UserSurface *s);
    
    MfxSurface *GetMfxSurface();
    
};

// struct VAData
// {
    // VA_DATA_TYPE type;
    // int refCount;
    // union
    // {
        // void *data;
        // // MfxSurface m_surface;
        // //struct UserSurface s;
        // uint8_t metaData[256];
    // }
// };
typedef std::list<VAData *> VADataPacket;
MfxSurface *GetMfxSurfacefromVAData(VaData *)
{
    
};

MfxSurface *GetUserSurfacefromVAData(VaData *)
{
    
};

