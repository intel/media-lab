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

#ifndef _ENCODEJPEG_THREAD_BLOCK_H_
#define _ENCODEJPEG_THREAD_BLOCK_H_

#include "ThreadBlock.h"
#include <mfxvideo++.h>
#include <mfxstructures.h>
#include <stdio.h>
#include <unistd.h>

enum VAEncodeType
{
    VAEncodeJpeg = 0,
    VAEncodeAvc
};

class EncodeThreadBlock : public VAThreadBlock
{
public:
    EncodeThreadBlock(uint32_t channel, VAEncodeType type);
    EncodeThreadBlock(uint32_t channel, VAEncodeType type, MFXVideoSession *ExtMfxSession, mfxFrameAllocator *mfxAllocator);

    ~EncodeThreadBlock();

    int Loop();

    inline void SetEncodeOutDump(bool flag = true) {m_encodeOutDump = flag; }

    inline void SetAsyncDepth(uint32_t depth) {m_asyncDepth = depth; }

    void SetInputFormat(uint32_t fourcc) {m_inputFormat = fourcc; }

    void SetInputResolution(uint32_t width, uint32_t height) {m_inputWidth = width; m_inputHeight = height;}

    inline void SetOutputRef(uint32_t count) {m_outputRef = count; }

protected:
    int PrepareInternal() override;

    bool CanBeProcessed(VAData *data);

    void PrepareJpeg();

    void PrepareAvc();

    void DumpOutput(uint8_t *data, uint32_t length, uint8_t channel, uint8_t frame);

    int FindFreeOutput();

    uint32_t m_channel;

    MFXVideoSession *m_mfxSession;
    mfxFrameAllocator *m_mfxAllocator;
    mfxBitstream m_mfxBS;
    MFXVideoENCODE *m_mfxEncode;

    mfxFrameSurface1 **m_encodeSurfaces;
    uint32_t m_encodeSurfNum;

    mfxVideoParam m_encParams;
    uint32_t m_asyncDepth;

    uint32_t m_inputFormat;
    uint32_t m_inputWidth;
    uint32_t m_inputHeight;

    bool m_encodeOutDump;
    int m_debugPrintCounter;
    FILE *m_fp;

    VAEncodeType m_encodeType;

    static const uint32_t m_bufferNum = 3;
    uint8_t *m_outBuffers[m_bufferNum];
    int m_outRefs[m_bufferNum];

    int m_outputRef;
};

#endif
