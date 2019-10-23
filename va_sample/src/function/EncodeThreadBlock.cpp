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
#include <EncodeThreadBlock.h>
#include "common.h"
#include <iostream>
#include <queue>
#include <map>

EncodeThreadBlock::EncodeThreadBlock(uint32_t channel):
    m_channel(channel),
    m_mfxSession (new MFXVideoSession()),
    m_mfxAllocator(new mfxFrameAllocator()),
    m_mfxEncode(nullptr),
    m_encodeSurfaces(nullptr),
    m_encodeSurfNum(0),
    m_asyncDepth(1),
    m_inputFormat(MFX_FOURCC_NV12),
    m_inputWidth(0),
    m_inputHeight(0),
    m_encodeOutDump(false),
    m_debugPrintCounter(0)
{
    memset(&m_encParams, 0, sizeof(m_encParams));

    mfxStatus sts;
    mfxIMPL impl;                 // SDK implementation type: hardware accelerator?, software? or else
    mfxVersion ver;               // media sdk version

    sts = MFX_ERR_NONE;
    impl = MFX_IMPL_AUTO_ANY;
    ver = { {0, 1} };
    sts = Initialize(impl, ver, m_mfxSession, m_mfxAllocator);
    if( sts != MFX_ERR_NONE)
    {
        printf("\t. Failed to initialize MediaSDK session\n");
    }
}

EncodeThreadBlock::EncodeThreadBlock(uint32_t channel, MFXVideoSession *mfxSession, mfxFrameAllocator *allocator):
    m_channel(channel),
    m_mfxSession (mfxSession),
    m_mfxAllocator(allocator),
    m_mfxEncode(nullptr),
    m_encodeSurfaces(nullptr),
    m_encodeSurfNum(0),
    m_asyncDepth(1),
    m_inputFormat(MFX_FOURCC_NV12),
    m_inputWidth(0),
    m_inputHeight(0),
    m_encodeOutDump(false),
    m_debugPrintCounter(0)
{
    memset(&m_encParams, 0, sizeof(m_encParams));
}

EncodeThreadBlock::~EncodeThreadBlock()
{
}

int EncodeThreadBlock::Prepare()
{
    mfxStatus sts;

    m_mfxEncode = new MFXVideoENCODE(*m_mfxSession);

    m_encParams.mfx.CodecId = MFX_CODEC_JPEG;
    m_encParams.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    memset(&m_encParams, 0, sizeof(m_encParams));
    m_encParams.mfx.CodecId = MFX_CODEC_JPEG;
    m_encParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

    // frame info parameters
    m_encParams.mfx.FrameInfo.FourCC       = m_inputFormat;
    m_encParams.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    m_encParams.mfx.FrameInfo.PicStruct    = MFX_PICSTRUCT_PROGRESSIVE;

    m_encParams.mfx.FrameInfo.Width  = MSDK_ALIGN16(m_inputWidth);
    m_encParams.mfx.FrameInfo.Height = MSDK_ALIGN16(m_inputHeight);

    m_encParams.mfx.FrameInfo.CropX = 0; //VPPParams.vpp.Out.CropX;
    m_encParams.mfx.FrameInfo.CropY = 0; //VPPParams.vpp.Out.CropY;
    m_encParams.mfx.FrameInfo.CropW = m_inputWidth;
    m_encParams.mfx.FrameInfo.CropH = m_inputHeight;

    m_encParams.mfx.Interleaved = 1;
    m_encParams.mfx.Quality = 95;
    m_encParams.mfx.RestartInterval = 0;
    m_encParams.mfx.CodecProfile = MFX_PROFILE_JPEG_BASELINE;
    m_encParams.mfx.NumThread = 1;
    memset(&m_encParams.mfx.reserved5,0,sizeof(m_encParams.mfx.reserved5));

    m_encParams.AsyncDepth = m_asyncDepth;

    mfxFrameAllocRequest EncRequest = { 0 };
    sts = m_mfxEncode->QueryIOSurf(&m_encParams, &EncRequest);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);

    // Prepare media sdk bit stream buffer
    // - Arbitrary buffer size for this example        
    memset(&m_mfxBS, 0, sizeof(m_mfxBS));
    m_mfxBS.MaxLength = m_encParams.mfx.FrameInfo.Width * m_encParams.mfx.FrameInfo.Height * 4;
    m_mfxBS.Data = new mfxU8[m_mfxBS.MaxLength];
    MSDK_CHECK_POINTER(m_mfxBS.Data, MFX_ERR_MEMORY_ALLOC);

    // Prepare media sdk decoder parameters
    //ReadBitStreamData();

    // memory allocation
    mfxFrameAllocResponse EncResponse = { 0 };
    sts = m_mfxAllocator->Alloc(m_mfxAllocator->pthis, &EncRequest, &EncResponse);
    if(MFX_ERR_NONE > sts)
    {
        MSDK_PRINT_RET_MSG(sts);
        return sts;
    }
    m_encodeSurfNum = EncResponse.NumFrameActual;
    // this surface will be used when decodes encoded stream
    m_encodeSurfaces = new mfxFrameSurface1 * [m_encodeSurfNum];
    if(!m_encodeSurfaces)
    {
        MSDK_PRINT_RET_MSG(MFX_ERR_MEMORY_ALLOC);            
        return -1;
    }

    for (int i = 0; i < m_encodeSurfNum; i++)
    {
        m_encodeSurfaces[i] = new mfxFrameSurface1;
        memset(m_encodeSurfaces[i], 0, sizeof(mfxFrameSurface1));
        memcpy(&(m_encodeSurfaces[i]->Info), &(m_encParams.mfx.FrameInfo), sizeof(mfxFrameInfo));

        // external allocator used - provide just MemIds
        m_encodeSurfaces[i]->Data.MemId = EncResponse.mids[i];
    }

    // Initialize the Media SDK encoder
    std::cout << "\t. Init Intel Media SDK Encoder" << std::endl;

    sts = m_mfxEncode->Init(&m_encParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);

    if(MFX_ERR_NONE > sts)
    {
        MSDK_PRINT_RET_MSG(sts);
    }

    printf("\t\t.. Support Jpeg Encoder and use video memory.\n");

    return 0;
}

bool EncodeThreadBlock::CanBeProcessed(VAData *data)
{
    if (data->Type() != MFX_SURFACE)
    {
        return false;
    }
    uint32_t w, h, p, format;
    data->GetSurfaceInfo(&w, &h, &p, &format);

    bool bResult = (w == m_encParams.mfx.FrameInfo.Width &&
                    h == m_encParams.mfx.FrameInfo.Height &&
                    format == m_inputFormat);

    return bResult;
}

int EncodeThreadBlock::Loop()
{
    mfxStatus sts = MFX_ERR_NONE;
    bool bNeedMore = false;
    mfxSyncPoint syncpEnc;
    mfxSyncPoint syncpDec;
    mfxSyncPoint syncpVPP;
    int nIndexDec = 0;
    int nIndexVpIn = 0;
    int nIndexVpOut = 0;
    uint32_t nDecoded = 0;
    std::queue<uint64_t> hasOutputs;
    std::map<uint64_t, VADataPacket *> recordedPackets;
    VAData *data = nullptr;

    while (true)
    {
        VADataPacket *InPacket = AcquireInput();
        //VADataPacket *OutPacket = DequeueOutput();

        // get all the inputs
        std::list<VAData *>vpOuts;
        for (auto ite = InPacket->begin(); ite != InPacket->end(); ite++)
        {
            data = *ite;
            if (CanBeProcessed(data))
            {
                vpOuts.push_back(data);
                //mfxFrameSurface1 *t = data->GetMfxSurface();
                //printf("ENC: Captured = %u\n", *((uint32_t*)(t->Data.MemId)) );
            }
            //else
            //{
            //    OutPacket->push_back(data);
            //}
        }
        ReleaseInput(InPacket);

        while ((MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts || MFX_ERR_MORE_SURFACE == sts))
        {
            if (vpOuts.empty())
            {
                printf("EncodeThreadBlock: EMPTY input!\n" );
                // This is not fatal error... lets try to continue
                sts = MFX_ERR_MORE_DATA;
                continue;
            }
            mfxFrameSurface1 *pInputSurface = vpOuts.front()->GetMfxSurface();
            sts = m_mfxEncode->EncodeFrameAsync(NULL, pInputSurface, &m_mfxBS, &syncpEnc);

            if (MFX_ERR_NONE < sts && !syncpEnc) // repeat the call if warning and no output
            {
                if (MFX_WRN_DEVICE_BUSY == sts)
                {
                    std::cout << std::endl << "> warning: MFX_WRN_DEVICE_BUSY" << std::endl;
                    usleep(1000); // wait if device is busy
                }
            }
            else if (MFX_ERR_NONE < sts && syncpEnc)
            {
                sts = MFX_ERR_NONE; // ignore warnings if output is available
            }

            if (MFX_ERR_NONE == sts)
            {
                sts = m_mfxSession->SyncOperation(syncpEnc, 60000); /* Encoder's Sync */
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                if (m_encodeOutDump)
                {
                    std::string file_dump_name = "va_sample_jpeg_debug_" + std::to_string(m_channel)
                             + "." + std::to_string(m_debugPrintCounter) + ".jpeg";
                    if(FILE *m_dump_bitstream_1 = std::fopen(file_dump_name.c_str(), "wb"))
                    {
                        std::fwrite(m_mfxBS.Data, sizeof(char),m_mfxBS.DataLength, m_dump_bitstream_1);
                        std::fclose(m_dump_bitstream_1);
                        m_debugPrintCounter++;
                    }
                } // if (m_encodeOutDump)
                m_mfxBS.DataLength = 0;
                data->DeRef(nullptr, 1);
                vpOuts.pop_front();
                break;
            }
        }
        // NO output for a while
        //EnqueueOutput(OutPacket);
    } // while(true)

    return 0;
}
