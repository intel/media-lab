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
#ifndef _CROP_THREAD_BLOCK_H_
#define _CROP_THREAD_BLOCK_H_

#include "ThreadBlock.h"
#include <mfxvideo++.h>
#include <mfxstructures.h>
#include <stdio.h>
#include <unistd.h>
#include <map>
#include "va/va.h"

class CropThreadBlock : public VAThreadBlock
{
public:
    CropThreadBlock(uint32_t index);

    ~CropThreadBlock();

    int Loop();

    inline void SetOutDump(bool flag = true) {m_dumpFlag = flag; }
    inline void SetOutResolution(uint32_t w, uint32_t h) {m_vpOutWidth = w; m_vpOutHeight = h; }
    inline void SetOutFormat(uint32_t format) {m_vpOutFormat = format; }

protected:
    int PrepareInternal() override;
    int Crop(mfxFrameAllocator *alloc,
             mfxFrameSurface1 *surf,
             uint8_t *dst,
             uint32_t srcx,
             uint32_t srcy,
             uint32_t srcw,
             uint32_t srch,
             bool keepRatio);

    bool IsDecodeOutput(VAData *data);
    bool IsRoiRegion(VAData *data);

    int FindFreeOutput();

    uint32_t m_index;

    uint32_t m_vpOutFormat;
    uint32_t m_vpOutWidth;
    uint32_t m_vpOutHeight;

    VASurfaceID m_outputVASurf;
    VAContextID m_contextID;

    static const uint32_t m_bufferNum = 10;
    uint8_t *m_outBuffers[m_bufferNum];
    int m_outRefs[m_bufferNum];

    bool m_dumpFlag;
};

#endif
