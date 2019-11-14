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
    IMAGENET_CLASS
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

    static VAData *Create(float left, float top, float right, float bottom, int c = -1, double conf = 1.0)
    {
        return new VAData(left, top, right, bottom, c, conf);
    }

    static VAData *Create(uint8_t *data, uint32_t offset, uint32_t length)
    {
        return new VAData(data, offset, length);
    }

    static VAData *Create(int c, float conf)
    {
        return new VAData(c, conf);
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
    int GetRoiRegion(float *left, float *top, float *right, float *bottom);
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
    VAData(float left, float top, float right, float bottom, int c, float conf);
    VAData(uint8_t *data, uint32_t offset, uint32_t length);
    VAData(int c, float conf);

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
    float m_left;
    float m_right;
    float m_top;
    float m_bottom;
    int m_class;
    float m_confidence;

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
