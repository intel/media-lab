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

#include "mfxstructures.h"

enum VA_DATA_TYPE
{
    USER_SURFACE,
    MFX_SURFACE,
    ROI_REGION,
};

class VADataCleaner
{
public:
    static VADataCleaner& getInstance()
    {
        static VADataCleaner instance;
        return instance;
    }
    ~VADataCleaner();

    void Add(VAData *data);
    void Destroy();
private:
    VADataCleaner();

    pthread_mutex_t m_mutex;
    std::list<VAData *>m_destroyList;

    pthread_t m_threadId;
};

class VAData
{
public:
    VAData(mfxFrameSurface1 *surface, mfxFrameAllocator *allocator);
    VAData(uint8_t *data, uint32_t w, uint32_t h, uint32_t p, uint32_t fourcc);
    VAData(uint32_t x, uint32_t y, uint32_t w, uint32_t h);

    ~VAData();
    
    mfxFrameSurface1 *GetMfxSurface();
    mfxFrameAllocator *GetMfxAllocator();
    uint8_t *GetSurfacePointer();

    // use external reference count to maintain the lifecycle
    // so external user can also track whether the VA data used or not.
    inline void SetExternalRef(int *ref) {m_ref = ref; }

    inline void AddRef(uint32_t count = 1) { *m_ref = *m_ref + count; }
    void DeRef(uint32_t count = 1);

    int GetSurfaceInfo(uint32_t *w, uint32_t *h, uint32_t *p, uint32_t *fourcc);
    int GetRoiRegion(uint32_t *x, uint32_t *y, uint32_t *w, uint32_t *h);

protected:
    VAData();
    
    // type
    VA_DATA_TYPE m_type;

    // data fields for surfaces
    mfxFrameSurface1 *m_mfxSurface;
    mfxFrameAllocator *m_mfxAllocator;
    uint8_t *m_data;
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_pitch;
    uint32_t m_fourcc;

    // data fields for Roi Region
    uint32_t m_x;
    uint32_t m_y;
    uint32_t m_w;
    uint32_t m_h;

    // reference control
    int m_internalRef;
    int *m_ref;
    pthread_mutex_t m_mutex;

private:
    VAData(const VAData &other);
    VAData &operator=(const VAData &other);
    
};

