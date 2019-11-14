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

#ifndef _DECODE_THREAD_BLOCK_H_
#define _DECODE_THREAD_BLOCK_H_

#include "ThreadBlock.h"
#include <mfxvideo++.h>
#include <mfxstructures.h>
#include <stdio.h>
#include <unistd.h>

class DecodeThreadBlock : public VAThreadBlock
{
public:
    DecodeThreadBlock(uint32_t channel);
    DecodeThreadBlock(uint32_t channel, MFXVideoSession *ExtMfxSession, mfxFrameAllocator *mfxAllocator);

    ~DecodeThreadBlock();

    void SetDecodeOutputRef(int ref) {m_decodeRefNum = ref; }
    void SetDecPostProc(bool flag)   {m_bEnableDecPostProc = flag; }

    void SetVPOutputRef(int ref) {m_vpRefNum = ref;}

    void SetVPRatio(uint32_t ratio) {m_vpRatio = ratio; }

    void SetVPOutFormat(uint32_t fourcc) {m_vpOutFormat = fourcc; }

    void SetVPOutResolution(uint32_t width, uint32_t height) {m_vpOutWidth = width; m_vpOutHeight = height;}

    int Loop();

    inline void SetVPOutDump(bool flag = true) {m_vpOutDump = flag; }
    inline void SetVPMemOutTypeVideo(bool flag = false) {m_vpMemOutTypeVideo = flag; }
    inline void SetDecodeOutputWithVP(bool flag = true) {m_decodeOutputWithVP = flag; }

    inline void GetDecodeResolution(uint32_t *width, uint32_t *height)
    {
        *width = m_decParams.mfx.FrameInfo.CropW;
        *height = m_decParams.mfx.FrameInfo.CropH;
    }

protected:
    int PrepareInternal() override;

    int ReadBitStreamData(); // fill the buffer in m_mfxBS, and store the remaining in m_buffer

    int GetFreeSurface(mfxFrameSurface1 **surfaces, int *refs, uint32_t count);
    
    uint32_t m_decodeRefNum;
    uint32_t m_vpRefNum;
    uint32_t m_channel;
    uint32_t m_vpRatio;
    bool     m_bEnableDecPostProc;

    MFXVideoSession *m_mfxSession;
    mfxFrameAllocator *m_mfxAllocator;
    mfxBitstream m_mfxBS;
    MFXVideoDECODE *m_mfxDecode;
    MFXVideoVPP *m_mfxVpp;

    mfxFrameSurface1 **m_decodeSurfaces;
    mfxFrameSurface1 *m_vpInSurface;
    mfxFrameSurface1 **m_vpOutSurfaces;
    uint8_t **m_vpOutBuffers;
    uint32_t m_decodeSurfNum;
    uint32_t m_vpInSurfNum;
    uint32_t m_vpOutSurfNum;

    mfxVideoParam m_decParams;
    mfxVideoParam m_vppParams;
    mfxExtDecVideoProcessing m_decVideoProcConfig;
    mfxExtVPPScaling m_scalingConfig;
    mfxExtBuffer *m_decodeExtBuf;
    mfxExtBuffer *m_vpExtBuf;

    uint8_t *m_buffer;
    uint32_t m_bufferOffset;
    uint32_t m_bufferLength;

    uint32_t m_vpOutFormat;
    uint32_t m_vpOutWidth;
    uint32_t m_vpOutHeight;

    int *m_decOutRefs;
    int *m_vpOutRefs;

    bool m_vpOutDump;
    bool m_vpMemOutTypeVideo;

    bool m_decodeOutputWithVP;
    
};

#endif
