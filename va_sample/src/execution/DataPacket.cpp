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
#include "DataPacket.h"
#include <unistd.h>

static void *VaDataCleanerFunc(void *arg)
{
    VADataCleaner *cleaner = static_cast<VADataCleaner *>(arg);
    while (cleaner->Continue())
    {
        cleaner->Destroy();
        usleep(100000);
    }
    return (void *)0;
}

VADataCleaner::VADataCleaner():
    m_continue(false),
    m_debug(false)
{
    pthread_mutex_init(&m_mutex, nullptr);
}

VADataCleaner::~VADataCleaner()
{
    Destroy();
    m_continue = false;
    pthread_join(m_threadId, nullptr);
    pthread_mutex_destroy(&m_mutex);
    
}

int VADataCleaner::Initialize(bool debug)
{
    m_continue = true;
    m_debug = debug;
    pthread_create(&m_threadId, nullptr, VaDataCleanerFunc, (void *)this);
}

void VADataCleaner::Destroy()
{
    pthread_mutex_lock(&m_mutex);
    while (!m_destroyList.empty())
    {
        VAData *data = m_destroyList.front();
        if (m_debug)
        {
            printf("VADataCleaner Thread: delete data %p, channel %d, frame %d\n", data, data->ChannelIndex(), data->FrameIndex());
        }
        delete data;
        m_destroyList.pop_front();
    }
    pthread_mutex_unlock(&m_mutex);
}

void VADataCleaner::Add(VAData *data)
{
    pthread_mutex_lock(&m_mutex);
    m_destroyList.push_back(data);
    pthread_mutex_unlock(&m_mutex);
}

VAData::VAData():
    m_mfxSurface(nullptr),
    m_mfxAllocator(nullptr),
    m_data(nullptr),
    m_width(0),
    m_height(0),
    m_pitch(0),
    m_fourcc(0),
    m_x(0),
    m_y(0),
    m_w(0),
    m_h(0),
    m_class(-1),
    m_confidence(1.0),
    m_internalRef(1),
    m_channelIndex(0),
    m_frameIndex(0)
{
    pthread_mutex_init(&m_mutex, nullptr);
    m_ref = &m_internalRef;
}

VAData::VAData(mfxFrameSurface1 *surface, mfxFrameAllocator *allocator):
    VAData()
{
    m_type = MFX_SURFACE;

    m_mfxSurface = surface;
    m_mfxAllocator = allocator;
    m_width = surface->Info.Width;
    m_height = surface->Info.Height;
    m_pitch = m_width;
    m_fourcc = surface->Info.FourCC;
}


VAData::VAData(uint8_t *data, uint32_t w, uint32_t h, uint32_t p, uint32_t fourcc):
    VAData()
{
    m_type = USER_SURFACE;

    m_data = data;
    m_width = w;
    m_height = h;
    m_pitch = p;
    m_fourcc = fourcc;
}

VAData::VAData(uint32_t x, uint32_t y, uint32_t w, uint32_t h, int c, double conf):
    VAData()
{
    m_type = ROI_REGION;

    m_x = x;
    m_y = y;
    m_w = w;
    m_h = h;

    m_class = c;
    m_confidence = conf;
}

VAData::VAData(uint8_t *data, uint32_t offset, uint32_t length):
    VAData()
{
    m_type = USER_BUFFER;

    m_data = data;
    m_offset = offset;
    m_length = length;
}


VAData::~VAData()
{
    pthread_mutex_destroy(&m_mutex);
}

mfxFrameSurface1 *VAData::GetMfxSurface()
{
    if (m_type != MFX_SURFACE)
    {
        return nullptr;
    }
    else
    {
        return m_mfxSurface;
    }
}

mfxFrameAllocator *VAData::GetMfxAllocator()
{
    if (m_type != MFX_SURFACE)
    {
        return nullptr;
    }
    else
    {
        return m_mfxAllocator;
    }

}

uint8_t *VAData::GetSurfacePointer()
{
    if (m_type != USER_SURFACE && m_type != USER_BUFFER)
    {
        return nullptr;
    }
    else
    {
        return m_data;
    }
}

void VAData::SetRef(uint32_t count)
{
    pthread_mutex_lock(&m_mutex);
    *m_ref = count;
    pthread_mutex_unlock(&m_mutex);
}

void VAData::DeRef(VADataPacket *packet, uint32_t count)
{
    pthread_mutex_lock(&m_mutex);
    *m_ref = *m_ref - count;
    if (*m_ref <= 0)
    {
        VADataCleaner::getInstance().Add(this);
    }
    else if (packet)
    {
        packet->push_back(this);
    }
    pthread_mutex_unlock(&m_mutex);
}

int VAData::GetSurfaceInfo(uint32_t *w, uint32_t *h, uint32_t *p, uint32_t *fourcc)
{
    *w = m_width;
    *h = m_height;
    *p = m_pitch;
    *fourcc = m_fourcc;
}

int VAData::GetRoiRegion(uint32_t *x, uint32_t *y, uint32_t *w, uint32_t *h)
{
    *x = m_x;
    *y = m_y;
    *w = m_w;
    *h = m_h;
}

int VAData::GetBufferInfo(uint32_t *offset, uint32_t *length)
{
    *offset = m_offset;
    *length = m_length;
}
