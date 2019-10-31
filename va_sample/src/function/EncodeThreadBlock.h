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

    int Prepare();

    int Loop();

    inline void SetEncodeOutDump(bool flag = true) {m_encodeOutDump = flag; }

    inline void SetAsyncDepth(uint32_t depth) {m_asyncDepth = depth; }

    void SetInputFormat(uint32_t fourcc) {m_inputFormat = fourcc; }

    void SetInputResolution(uint32_t width, uint32_t height) {m_inputWidth = width; m_inputHeight = height;}

protected:

    bool CanBeProcessed(VAData *data);

    void PrepareJpeg();

    void PrepareAvc();

    void DumpOutput(uint8_t *data, uint32_t length, uint8_t channel, uint8_t frame);

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
};

#endif
