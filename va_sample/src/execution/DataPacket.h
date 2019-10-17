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

#include <list>
#include "mfxstructures.h"
#include <mfxvideo++.h>

class VAData;
typedef std::list<VAData *> VADataPacket;

enum VA_DATA_TYPE
{
    USER_SURFACE,
    MFX_SURFACE,
    ROI_REGION,
    USER_BUFFER,
};

inline uint64_t ID(uint32_t c, uint32_t f)
{
    return f | ((uint64_t)c << 32);
}

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

    int Initialize(bool debug = false);

    inline bool Continue() {return m_continue; }
private:
    VADataCleaner();

    pthread_mutex_t m_mutex;
    std::list<VAData *>m_destroyList;

    pthread_t m_threadId;
    bool m_continue;
    bool m_debug; //print logs
};

class VAData
{
friend class VADataCleaner;
public:
    // No need to destory these created VAData explictly
    // there is cleaner for this
    static VAData *Create(mfxFrameSurface1 *surface, mfxFrameAllocator *allocator)
    {
        return new VAData(surface, allocator);
    }

    static VAData *Create(uint8_t *data, uint32_t w, uint32_t h, uint32_t p, uint32_t fourcc)
    {
        return new VAData(data, w, h, p, fourcc);
    }

    static VAData *Create(uint32_t x, uint32_t y, uint32_t w, uint32_t h, int c = -1, double conf = 1.0)
    {
        return new VAData(x, y, w, h, c, conf);
    }

    static VAData *Create(uint8_t *data, uint32_t offset, uint32_t length)
    {
        return new VAData(data, offset, length);
    }
    
    mfxFrameSurface1 *GetMfxSurface();
    mfxFrameAllocator *GetMfxAllocator();
    uint8_t *GetSurfacePointer();

    // use external reference count to maintain the lifecycle
    // so external user can also track whether the VA data used or not.
    inline void SetExternalRef(int *ref) {m_ref = ref; }

    inline VA_DATA_TYPE Type() {return m_type; }

    void SetRef(uint32_t count = 1);
    void DeRef(VADataPacket *packet = nullptr, uint32_t count = 1);

    int GetSurfaceInfo(uint32_t *w, uint32_t *h, uint32_t *p, uint32_t *fourcc);
    int GetRoiRegion(uint32_t *x, uint32_t *y, uint32_t *w, uint32_t *h);
    int GetBufferInfo(uint32_t *offset, uint32_t *length);

    inline void SetID(uint32_t channel, uint32_t frame) {m_channelIndex = channel; m_frameIndex = frame; }
    inline void SetRoiIndex(uint32_t index) {m_roiIndex = index; }
    inline uint32_t FrameIndex() {return m_frameIndex; }
    inline uint32_t ChannelIndex() {return m_channelIndex; }
    inline uint32_t RoiIndex() {return m_roiIndex; }

    inline int Ref() {return *m_ref;}

    inline double Confidence() {return m_confidence; }
    inline int Class() {return m_class; }

protected:
    VAData();
    VAData(mfxFrameSurface1 *surface, mfxFrameAllocator *allocator);
    VAData(uint8_t *data, uint32_t w, uint32_t h, uint32_t p, uint32_t fourcc);
    VAData(uint32_t x, uint32_t y, uint32_t w, uint32_t h, int c, double conf);
    VAData(uint8_t *data, uint32_t offset, uint32_t length);

    ~VAData();
    
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
    int m_class;
    double m_confidence;

    // data fields for buffer
    uint32_t m_offset;
    uint32_t m_length;

    // reference control
    int m_internalRef;
    int *m_ref;
    pthread_mutex_t m_mutex;

    // ID of the packet
    uint32_t m_channelIndex;
    uint32_t m_frameIndex;
    uint32_t m_roiIndex;

private:
    VAData(const VAData &other);
    VAData &operator=(const VAData &other);
    
};


#endif
