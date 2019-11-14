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
