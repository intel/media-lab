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
#include "CropThreadBlock.h"
#include "common.h"

extern VADisplay m_va_dpy;

#define CHECK_VASTATUS(va_status,func)                                      \
    if (va_status != VA_STATUS_SUCCESS) {                                     \
        fprintf(stderr,"%s:%s (%d) failed,exit\n", __func__, func, __LINE__); \
        return -1;                                                              \
    }

CropThreadBlock::CropThreadBlock(uint32_t index):
    m_index(index),
    m_vpOutFormat(MFX_FOURCC_RGBP),
    m_vpOutWidth(224),
    m_vpOutHeight(224),
    m_outputVASurf(VA_INVALID_ID),
    m_contextID(0),
    m_dumpFlag(false)
{
    memset(m_outBuffers, 0, sizeof(m_outBuffers));
    memset(m_outRefs, 0, sizeof(m_outRefs));
}

CropThreadBlock::~CropThreadBlock()
{
    if (m_outputVASurf != VA_INVALID_ID)
    {
        vaDestroySurfaces(m_va_dpy, &m_outputVASurf, 1);
    }
    for (int i = 0; i < m_bufferNum; i++)
    {
        if (m_outBuffers[i])
        {
            delete[] m_outBuffers[i];
            m_outBuffers[i] = nullptr;
        }
    }
}

int CropThreadBlock::Prepare()
{
    if (m_va_dpy == nullptr)
    {
        printf("Error: Libva not initialized\n");
        return -1;
    }

    VAConfigID  config_id = 0;

    // allocate output surface
    VAStatus vaStatus;
    VASurfaceAttrib    surface_attrib;
    surface_attrib.type =  VASurfaceAttribPixelFormat;
    surface_attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
    surface_attrib.value.type = VAGenericValueTypeInteger;
    surface_attrib.value.value.i = m_vpOutFormat;
    uint32_t format = VA_RT_FORMAT_YUV420;
    if (m_vpOutFormat == MFX_FOURCC_RGBP)
    {
        format = VA_RT_FORMAT_RGBP;
    }
    vaStatus = vaCreateSurfaces(m_va_dpy,
        format,
        m_vpOutWidth,
        m_vpOutHeight,
        &m_outputVASurf,
        1,
        &surface_attrib,
        1);

    // Create config
    VAConfigAttrib attrib;
    attrib.type = VAConfigAttribRTFormat;
    vaStatus = vaGetConfigAttributes(m_va_dpy,
        VAProfileNone,
        VAEntrypointVideoProc,
        &attrib,
        1);
    CHECK_VASTATUS(vaStatus, "vaGetConfigAttributes");
    vaStatus = vaCreateConfig(m_va_dpy,
                                VAProfileNone,
                                VAEntrypointVideoProc,
                                &attrib,
                                1,
                                &config_id);
    CHECK_VASTATUS(vaStatus, "vaCreateConfig");

    vaStatus = vaCreateContext(m_va_dpy,
                                config_id,
                                m_vpOutWidth,
                                m_vpOutHeight,
                                VA_PROGRESSIVE,
                                &m_outputVASurf,
                                1,
                                &m_contextID);
    CHECK_VASTATUS(vaStatus, "vaCreateContext");

    // allocate output buffers
    for (int i = 0; i < m_bufferNum; i++)
    {
        m_outBuffers[i] = new uint8_t[m_vpOutWidth * m_vpOutHeight * 3];
    }
                                
    return 0;
}

int CropThreadBlock::FindFreeOutput()
{
    for (int i = 0; i < m_bufferNum; i ++)
    {
        if (m_outRefs[i] <= 0)
        {
            return i;
        }
    }
    // no free output found
    return -1;
}

int CropThreadBlock::Loop()
{
    // dump the cropping output for test
    while(true)
    {
        VADataPacket *input = AcquireInput();
        VADataPacket *output = DequeueOutput();
        VAData *decodeOutput = nullptr;
        std::vector<VAData *> rois;
        for (auto ite = input->begin(); ite != input->end(); ite++)
        {
            VAData *data = *ite;
            if (IsDecodeOutput(data))
            {
                decodeOutput = data;
            }
            else if (IsRoiRegion(data))
            {
                rois.push_back(data);
            }
            else
            {
                output->push_back(data);
            }
        }

        ReleaseInput(input);

        uint32_t decodeWidth, decodeHeight, p, f;
        decodeOutput->GetSurfaceInfo(&decodeWidth, &decodeHeight, &p, &f);

        //printf("in Crop, %d rois\n", rois.size());
        for (int i = 0; i < rois.size(); i++)
        {
            VAData *roi = rois[i];
            float l, r, t, b;
            roi->GetRoiRegion(&l, &t, &r, &b);

            int index = -1;
            while (index == -1)
            {
                index = FindFreeOutput();
                if (index == -1)
                    usleep(10000);
            }
            Crop(decodeOutput->GetMfxAllocator(),
                 decodeOutput->GetMfxSurface(),
                 m_outBuffers[index],
                 (uint32_t)(l * decodeWidth),
                 (uint32_t)(t * decodeHeight),
                 (uint32_t)((r - l) * decodeWidth),
                 (uint32_t)((b - t) * decodeHeight),
                 true);

            VAData *cropOut = VAData::Create(m_outBuffers[index], m_vpOutWidth, m_vpOutHeight, m_vpOutWidth, m_vpOutFormat);
            cropOut->SetID(roi->ChannelIndex(), roi->FrameIndex());
            cropOut->SetRoiIndex(roi->RoiIndex());
            cropOut->SetExternalRef(&m_outRefs[index]);
            cropOut->SetRef(1);
            roi->DeRef(output);
            output->push_back(cropOut);

            if (m_dumpFlag)
            {
                char filename[256];
                sprintf(filename, "CropOut_%d.%dx%d.rgbp", roi->ChannelIndex(), m_vpOutWidth, m_vpOutHeight);
                FILE *fp = fopen(filename, "ab");
                fwrite(m_outBuffers[index], 1, m_vpOutWidth*m_vpOutHeight*3, fp);
                fclose(fp);
            }
        }

        decodeOutput->DeRef(output);
        EnqueueOutput(output);
    }
}

int CropThreadBlock::Crop(mfxFrameAllocator *alloc,
             mfxFrameSurface1 *surf,
             uint8_t *dst,
             uint32_t srcx,
             uint32_t srcy,
             uint32_t srcw,
             uint32_t srch,
             bool keepRatio)
{
    // Get Input VASurface
    mfxHDL handle;
    alloc->GetHDL(alloc->pthis, surf->Data.MemId, &(handle));
    VASurfaceID inputSurf = *(VASurfaceID *)handle;

    // prepare VP parameters
    VAStatus va_status;
    VAProcPipelineParameterBuffer pipeline_param;
    VARectangle surface_region, output_region;
    VABufferID pipeline_param_buf_id = VA_INVALID_ID;
    VABufferID filter_param_buf_id = VA_INVALID_ID;

    uint dstw, dsth;
    if (keepRatio)
    {
        double ratio = (double)srcw/(double)srch;
        double ratio2 = (double)m_vpOutWidth/(double)m_vpOutHeight;
        if (ratio > ratio2)
        {
            dstw = m_vpOutWidth;
            dsth = (uint32_t)((double)dstw/ratio);
        }
        else
        {
            dsth = m_vpOutHeight;
            dstw = (uint32_t)((double)dsth * ratio);
        }
    }
    else
    {
        dstw = m_vpOutWidth;
        dsth = m_vpOutHeight;
    }
    surface_region.x = srcx;
    surface_region.y = srcy;
    surface_region.width = srcw;
    surface_region.height = srch;
    output_region.x = 0;
    output_region.y = 0;
    output_region.width = dstw;
    output_region.height = dsth;
    memset(&pipeline_param, 0, sizeof(pipeline_param));
    pipeline_param.surface = inputSurf;
    pipeline_param.surface_region = &surface_region;
    pipeline_param.output_region = &output_region;
    pipeline_param.output_background_color = 0xff000000;
    pipeline_param.filter_flags = 0;
    pipeline_param.filters      = &filter_param_buf_id;
    pipeline_param.num_filters  = 0;

    va_status = vaCreateBuffer(m_va_dpy,
                               m_contextID,
                               VAProcPipelineParameterBufferType,
                               sizeof(pipeline_param),
                               1,
                               &pipeline_param,
                               &pipeline_param_buf_id);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    va_status = vaBeginPicture(m_va_dpy,
                               m_contextID,
                               m_outputVASurf);
    CHECK_VASTATUS(va_status, "vaBeginPicture");

    va_status = vaRenderPicture(m_va_dpy,
                                m_contextID,
                                &pipeline_param_buf_id,
                                1);
    CHECK_VASTATUS(va_status, "vaRenderPicture");

    va_status = vaEndPicture(m_va_dpy, m_contextID);
    CHECK_VASTATUS(va_status, "vaEndPicture");

    if (filter_param_buf_id != VA_INVALID_ID)
        vaDestroyBuffer(m_va_dpy,filter_param_buf_id);
    if (pipeline_param_buf_id != VA_INVALID_ID)
        vaDestroyBuffer(m_va_dpy,pipeline_param_buf_id);

    VAImage surface_image;
    void *surface_p = nullptr;
    va_status = vaDeriveImage(m_va_dpy, m_outputVASurf, &surface_image);
    CHECK_VASTATUS(va_status, "vaDeriveImage");
    va_status = vaMapBuffer(m_va_dpy, surface_image.buf, &surface_p);
    CHECK_VASTATUS(va_status, "vaMapBuffer");

    uint8_t *y_dst = dst;
    uint8_t *y_src = (uint8_t *)surface_p;

    for (int i = 0; i < surface_image.num_planes; i ++)
    {
        y_src = (uint8_t *)surface_p + surface_image.offsets[i];
        for (int row = 0; row < surface_image.height; row++)
        {
            memcpy(y_dst, y_src, surface_image.width);
            y_src += surface_image.pitches[i];
            y_dst += surface_image.width;
        }
    }

    vaUnmapBuffer(m_va_dpy, surface_image.buf);
    vaDestroyImage(m_va_dpy, surface_image.image_id);
}

bool CropThreadBlock::IsDecodeOutput(VAData *data)
{
    return data->Type() == MFX_SURFACE;
}

bool CropThreadBlock::IsRoiRegion(VAData *data)
{
    return data->Type() == ROI_REGION;
}

