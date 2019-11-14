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

#include "common.h"

mfxStatus Initialize(mfxIMPL impl, mfxVersion ver, MFXVideoSession* pSession, mfxFrameAllocator* pmfxAllocator, bool bCreateSharedHandles)
{
    mfxStatus sts = MFX_ERR_NONE;

    // Initialize Intel Media SDK Session
    sts = pSession->Init(impl, &ver);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);


    // Create VA display
    mfxHDL displayHandle = { 0 };
    sts = CreateVAEnvDRM(&displayHandle);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Provide VA display handle to Media SDK
    sts = pSession->SetHandle(static_cast < mfxHandleType >(MFX_HANDLE_VA_DISPLAY), displayHandle);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // If mfxFrameAllocator is provided it means we need to setup  memory allocator
    if (pmfxAllocator) {
        pmfxAllocator->pthis  = *pSession; // We use Media SDK session ID as the allocation identifier
        pmfxAllocator->Alloc  = simple_alloc;
        pmfxAllocator->Free   = simple_free;
        pmfxAllocator->Lock   = simple_lock;
        pmfxAllocator->Unlock = simple_unlock;
        pmfxAllocator->GetHDL = simple_gethdl;

        // Since we are using video memory we must provide Media SDK with an external allocator
        sts = pSession->SetFrameAllocator(pmfxAllocator);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    return sts;
}

void PrintErrString(int err,const char* filestr,int line)
{
    switch (err) {
    case   0:
        printf("\n No error.\n");
        break;
    case  -1:
        printf("\n Unknown error: %s %d\n",filestr,line);
        break;
    case  -2:
        printf("\n Null pointer.  Check filename/path + permissions? %s %d\n",filestr,line);
        break;
    case  -3:
        printf("\n Unsupported feature/library load error. %s %d\n",filestr,line);
        break;
    case  -4:
        printf("\n Could not allocate memory. %s %d\n",filestr,line);
        break;
    case  -5:
        printf("\n Insufficient IO buffers. %s %d\n",filestr,line);
        break;
    case  -6:
        printf("\n Invalid handle. %s %d\n",filestr,line);
        break;
    case  -7:
        printf("\n Memory lock failure. %s %d\n",filestr,line);
        break;
    case  -8:
        printf("\n Function called before initialization. %s %d\n",filestr,line);
        break;
    case  -9:
        printf("\n Specified object not found. %s %d\n",filestr,line);
        break;
    case -10:
        printf("\n More input data expected. %s %d\n",filestr,line);
        break;
    case -11:
        printf("\n More output surfaces expected. %s %d\n",filestr,line);
        break;
    case -12:
        printf("\n Operation aborted. %s %d\n",filestr,line);
        break;
    case -13:
        printf("\n HW device lost. %s %d\n",filestr,line);
        break;
    case -14:
        printf("\n Incompatible video parameters. %s %d\n",filestr,line);
        break;
    case -15:
        printf("\n Invalid video parameters. %s %d\n",filestr,line);
        break;
    case -16:
        printf("\n Undefined behavior. %s %d\n",filestr,line);
        break;
    case -17:
        printf("\n Device operation failure. %s %d\n",filestr,line);
        break;
    case -18:
        printf("\n More bitstream data expected. %s %d\n",filestr,line);
        break;
    case -19:
        printf("\n Incompatible audio parameters. %s %d\n",filestr,line);
        break;
    case -20:
        printf("\n Invalid audio parameters. %s %d\n",filestr,line);
        break;
    default:
        printf("\nError code %d,\t%s\t%d\n\n", err, filestr, line);
    }
}

mfxStatus ReadPlaneData(mfxU16 w, mfxU16 h, mfxU8* buf, mfxU8* ptr,
                        mfxU16 pitch, mfxU16 offset, FILE* fSource)
{
    mfxU32 nBytesRead;
    for (mfxU16 i = 0; i < h; i++) {
        nBytesRead = (mfxU32) fread(buf, 1, w, fSource);
        if (w != nBytesRead)
            return MFX_ERR_MORE_DATA;
        for (mfxU16 j = 0; j < w; j++)
            ptr[i * pitch + j * 2 + offset] = buf[j];
    }
    return MFX_ERR_NONE;
}

mfxStatus ReadBitStreamData(mfxBitstream* pBS, FILE* fSource)
{
    memmove(pBS->Data, pBS->Data + pBS->DataOffset, pBS->DataLength);
    pBS->DataOffset = 0;

    mfxU32 nBytesRead = (mfxU32) fread(pBS->Data + pBS->DataLength, 1,
                                       pBS->MaxLength - pBS->DataLength,
                                       fSource);

    if (0 == nBytesRead)
        return MFX_ERR_MORE_DATA;

    pBS->DataLength += nBytesRead;

    return MFX_ERR_NONE;
}

mfxStatus WriteSection(mfxU8* plane, mfxU16 factor, mfxU16 chunksize,
                       mfxFrameInfo* pInfo, mfxFrameData* pData, mfxU32 i,
                       mfxU32 j, FILE* fSink)
{
    if (chunksize !=
        fwrite(plane +
               (pInfo->CropY * pData->Pitch / factor + pInfo->CropX) +
               i * pData->Pitch + j, 1, chunksize, fSink))
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    return MFX_ERR_NONE;
}

// Get free raw frame surface
int GetFreeSurfaceIndex(mfxFrameSurface1** pSurfacesPool, mfxU16 nPoolSize)
{
    if (pSurfacesPool)
        for (mfxU16 i = 0; i < nPoolSize; i++)
            if (0 == pSurfacesPool[i]->Data.Locked)
                return i;
    return MFX_ERR_NOT_FOUND;
}

#include<map>
#ifndef MFX_FOURCC_RGBP
#define MFX_FOURCC_RGBP  MFX_MAKEFOURCC('R','G','B','P')
#endif


struct sharedResponse {
    mfxFrameAllocResponse mfxResponse;
    int refCount;
};

std::map<mfxMemId*, mfxHDL>      allocResponses;
std::map<mfxHDL, sharedResponse> allocDecodeResponses;


mfxStatus va_to_mfx_status(VAStatus va_res)
{
    mfxStatus mfxRes = MFX_ERR_NONE;

    switch (va_res) {
    case VA_STATUS_SUCCESS:
        mfxRes = MFX_ERR_NONE;
        break;
    case VA_STATUS_ERROR_ALLOCATION_FAILED:
        mfxRes = MFX_ERR_MEMORY_ALLOC;
        break;
    case VA_STATUS_ERROR_ATTR_NOT_SUPPORTED:
    case VA_STATUS_ERROR_UNSUPPORTED_PROFILE:
    case VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT:
    case VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT:
    case VA_STATUS_ERROR_UNSUPPORTED_BUFFERTYPE:
    case VA_STATUS_ERROR_FLAG_NOT_SUPPORTED:
    case VA_STATUS_ERROR_RESOLUTION_NOT_SUPPORTED:
        mfxRes = MFX_ERR_UNSUPPORTED;
        break;
    case VA_STATUS_ERROR_INVALID_DISPLAY:
    case VA_STATUS_ERROR_INVALID_CONFIG:
    case VA_STATUS_ERROR_INVALID_CONTEXT:
    case VA_STATUS_ERROR_INVALID_SURFACE:
    case VA_STATUS_ERROR_INVALID_BUFFER:
    case VA_STATUS_ERROR_INVALID_IMAGE:
    case VA_STATUS_ERROR_INVALID_SUBPICTURE:
        mfxRes = MFX_ERR_NOT_INITIALIZED;
        break;
    case VA_STATUS_ERROR_INVALID_PARAMETER:
        mfxRes = MFX_ERR_INVALID_VIDEO_PARAM;
    default:
        mfxRes = MFX_ERR_UNKNOWN;
        break;
    }
    return mfxRes;
}


// global variables shared by the below functions

// VAAPI display handle
VADisplay m_va_dpy = NULL;
// gfx card file descriptor
int m_fd = -1;
// decoder surfaces chain shared between app and MSDK

mfxStatus CreateVAEnvDRM(mfxHDL* displayHandle)
{
  VAStatus va_res = VA_STATUS_SUCCESS;
  mfxStatus sts = MFX_ERR_NONE;
  int major_version = 0, minor_version = 0;

  if(m_va_dpy != NULL)
  {
       *displayHandle = m_va_dpy;
        sts=MFX_ERR_NONE;
        return sts;
  }  

  //search for valid graphics device                                
  for (int adapter_num=0;adapter_num<6;adapter_num++)
  {

    char adapterpath[256];

    sts = MFX_ERR_NOT_INITIALIZED;

    if (adapter_num<3) {
      snprintf(adapterpath,sizeof(adapterpath),"/dev/dri/renderD%d",
	       adapter_num+128);
    } else {
      snprintf(adapterpath,sizeof(adapterpath),"/dev/dri/card%d",
	       adapter_num-3);
    }

    printf("opening %s\n",adapterpath); fflush(stdout);

    m_fd = open(adapterpath, O_RDWR);

    if (m_fd < 0) continue;
    
    m_va_dpy = vaGetDisplayDRM(m_fd);

    if (!m_va_dpy) {
      close(m_fd);
      m_fd=-1;
      continue;
    }

    va_res = vaInitialize(m_va_dpy, &major_version, &minor_version);

    if (VA_STATUS_SUCCESS != va_res) {
      close(m_fd);
      m_fd = -1;
      continue;
    }
    else
      {
	*displayHandle = m_va_dpy;
	sts=MFX_ERR_NONE;
	break;
      }
  }
  
  if (MFX_ERR_NONE != sts) {
        throw std::bad_alloc();
  } 
  
  return sts;
}

//utiility function to convert MFX fourcc to VA format
unsigned int ConvertMfxFourccToVAFormat(mfxU32 fourcc)
{
    switch (fourcc) {
    case MFX_FOURCC_NV12:
        return VA_FOURCC_NV12;
    case MFX_FOURCC_YUY2:
        return VA_FOURCC_YUY2;
    case MFX_FOURCC_YV12:
        return VA_FOURCC_YV12;
    case MFX_FOURCC_RGB4:
        return VA_FOURCC_ARGB;
    case MFX_FOURCC_RGBP:
        return VA_FOURCC_RGBP;
    case MFX_FOURCC_P8:
        return VA_FOURCC_P208;

    default:
        assert(!"unsupported fourcc");
        return 0;
    }
}

// VAAPI Allocator internal Mem ID
struct vaapiMemId {
    VASurfaceID* m_surface;
    VAImage m_image;
    // variables for VAAPI Allocator inernal color convertion
    unsigned int m_fourcc;
    mfxU8* m_sys_buffer;
    mfxU8* m_va_buffer;
};

//
// Media SDK memory allocator entrypoints....
//

mfxStatus _simple_alloc(mfxFrameAllocRequest* request,
                        mfxFrameAllocResponse* response)
{
    mfxStatus mfx_res = MFX_ERR_NONE;
    VAStatus va_res = VA_STATUS_SUCCESS;
    unsigned int va_fourcc = 0;
    VASurfaceID* surfaces = NULL;
    VASurfaceAttrib attrib;
    vaapiMemId* vaapi_mids = NULL, *vaapi_mid = NULL;
    mfxMemId* mids = NULL;
    mfxU32 fourcc = request->Info.FourCC;
    mfxU16 surfaces_num = request->NumFrameSuggested, numAllocated = 0, i =
                              0;
    bool bCreateSrfSucceeded = false;

    memset(response, 0, sizeof(mfxFrameAllocResponse));

    va_fourcc = ConvertMfxFourccToVAFormat(fourcc);
    if (!va_fourcc || ((VA_FOURCC_NV12 != va_fourcc) &&
                       (VA_FOURCC_YV12 != va_fourcc) &&
                       (VA_FOURCC_YUY2 != va_fourcc) &&
                       (VA_FOURCC_ARGB != va_fourcc) &&
                       (VA_FOURCC_RGBP != va_fourcc) &&
                       (VA_FOURCC_P208 != va_fourcc))) {
        return MFX_ERR_MEMORY_ALLOC;
    }

    if (!surfaces_num) {
        return MFX_ERR_MEMORY_ALLOC;
    }
    if (MFX_ERR_NONE == mfx_res) {
        surfaces =
            (VASurfaceID*) calloc(surfaces_num, sizeof(VASurfaceID));
        vaapi_mids =
            (vaapiMemId*) calloc(surfaces_num, sizeof(vaapiMemId));
        mids = (mfxMemId*) calloc(surfaces_num, sizeof(mfxMemId));
        if ((NULL == surfaces) || (NULL == vaapi_mids)
            || (NULL == mids))
            mfx_res = MFX_ERR_MEMORY_ALLOC;
    }
    if (MFX_ERR_NONE == mfx_res) {
        if (VA_FOURCC_P208 != va_fourcc) {
            attrib.type = VASurfaceAttribPixelFormat;
            attrib.value.type = VAGenericValueTypeInteger;
            attrib.value.value.i = va_fourcc;
            attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
printf("4 \r\n");
            va_res = vaCreateSurfaces(m_va_dpy,
                                      VA_RT_FORMAT_YUV420,
                                      request->Info.Width,
                                      request->Info.Height,
                                      surfaces, surfaces_num,
                                      &attrib, 1);
            mfx_res = va_to_mfx_status(va_res);
printf("5 \r\n");         
   bCreateSrfSucceeded = (MFX_ERR_NONE == mfx_res);
        } else {
            VAContextID context_id = request->reserved[0];
            int codedbuf_size = (request->Info.Width * request->Info.Height) * 400 / (16 * 16);     //from libva spec

            for (numAllocated = 0; numAllocated < surfaces_num;
                 numAllocated++) {
                VABufferID coded_buf;

                va_res = vaCreateBuffer(m_va_dpy,
                                        context_id,
                                        VAEncCodedBufferType,
                                        codedbuf_size,
                                        1, NULL, &coded_buf);
                mfx_res = va_to_mfx_status(va_res);
                if (MFX_ERR_NONE != mfx_res)
                    break;
                surfaces[numAllocated] = coded_buf;
            }
        }
    }
    if (MFX_ERR_NONE == mfx_res) {
        for (i = 0; i < surfaces_num; ++i) {
            vaapi_mid = &(vaapi_mids[i]);
            vaapi_mid->m_fourcc = fourcc;
            vaapi_mid->m_surface = &(surfaces[i]);
            mids[i] = vaapi_mid;
        }
    }
    if (MFX_ERR_NONE == mfx_res) {
        response->mids = mids;
        response->NumFrameActual = surfaces_num;
    } else {                // i.e. MFX_ERR_NONE != mfx_res
        response->mids = NULL;
        response->NumFrameActual = 0;
        if (VA_FOURCC_P208 != va_fourcc) {
            if (bCreateSrfSucceeded)
                vaDestroySurfaces(m_va_dpy, surfaces,
                                  surfaces_num);
        } else {
            for (i = 0; i < numAllocated; i++)
                vaDestroyBuffer(m_va_dpy, surfaces[i]);
        }
        if (mids) {
            free(mids);
            mids = NULL;
        }
        if (vaapi_mids) {
            free(vaapi_mids);
            vaapi_mids = NULL;
        }
        if (surfaces) {
            free(surfaces);
            surfaces = NULL;
        }
    }
printf("done mfx_res=%d\r\n", mfx_res);
    return mfx_res;
}

mfxStatus simple_alloc(mfxHDL pthis, mfxFrameAllocRequest* request,
                       mfxFrameAllocResponse* response)
{
    mfxStatus sts = MFX_ERR_NONE;

    if (0 == request || 0 == response || 0 == request->NumFrameSuggested)
        return MFX_ERR_MEMORY_ALLOC;

    if ((request->Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET |
                          MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET)) == 0)
        return MFX_ERR_UNSUPPORTED;

    if (request->NumFrameSuggested <=
        allocDecodeResponses[pthis].mfxResponse.NumFrameActual
        && MFX_MEMTYPE_EXTERNAL_FRAME & request->Type
        && MFX_MEMTYPE_FROM_DECODE & request->Type
        &&allocDecodeResponses[pthis].mfxResponse.NumFrameActual != 0) {
        // Memory for this request was already allocated during manual allocation stage. Return saved response
        //   When decode acceleration device (VAAPI) is created it requires a list of VAAPI surfaces to be passed.
        //   Therefore Media SDK will ask for the surface info/mids again at Init() stage, thus requiring us to return the saved response
        //   (No such restriction applies to Encode or VPP)

        *response = allocDecodeResponses[pthis].mfxResponse;
        allocDecodeResponses[pthis].refCount++;

    } else {
        sts = _simple_alloc(request, response);

        if (MFX_ERR_NONE == sts) {
            if (MFX_MEMTYPE_EXTERNAL_FRAME & request->Type &&
                MFX_MEMTYPE_FROM_DECODE & request->Type) {
                // Decode alloc response handling
                allocDecodeResponses[pthis].mfxResponse = *response;
                allocDecodeResponses[pthis].refCount++;
                //allocDecodeRefCount[pthis]++;
            } else {
                // Encode and VPP alloc response handling
                allocResponses[response->mids] = pthis;
            }
        }
    }

    return sts;
}

mfxStatus simple_lock(mfxHDL pthis, mfxMemId mid, mfxFrameData* ptr)
{
    mfxStatus mfx_res = MFX_ERR_NONE;
    VAStatus va_res = VA_STATUS_SUCCESS;
    vaapiMemId* vaapi_mid = (vaapiMemId*) mid;
    mfxU8* pBuffer = 0;

    if (!vaapi_mid || !(vaapi_mid->m_surface))
        return MFX_ERR_INVALID_HANDLE;


    if (MFX_FOURCC_P8 == vaapi_mid->m_fourcc) {     // bitstream processing
        VACodedBufferSegment* coded_buffer_segment;
        va_res =
            vaMapBuffer(m_va_dpy, *(vaapi_mid->m_surface),
                        (void**)(&coded_buffer_segment));
        mfx_res = va_to_mfx_status(va_res);
        ptr->Y = (mfxU8*) coded_buffer_segment->buf;
    } else {                // Image processing
        va_res = vaSyncSurface(m_va_dpy, *(vaapi_mid->m_surface));
     //   mfx_res = va_to_mfx_status(va_res);
        if (MFX_ERR_NONE == mfx_res) {
            va_res =
                vaDeriveImage(m_va_dpy, *(vaapi_mid->m_surface),
                              &(vaapi_mid->m_image));
            mfx_res = va_to_mfx_status(va_res);
        }
        if (MFX_ERR_NONE == mfx_res) {
            va_res =
                vaMapBuffer(m_va_dpy, vaapi_mid->m_image.buf,
                            (void**)&pBuffer);
            mfx_res = va_to_mfx_status(va_res);
        }
        if (MFX_ERR_NONE == mfx_res) {
            switch (vaapi_mid->m_image.format.fourcc) {
            case VA_FOURCC_NV12:
                if (vaapi_mid->m_fourcc == MFX_FOURCC_NV12) {
                    ptr->Pitch =
                        (mfxU16) vaapi_mid->
                        m_image.pitches[0];
                    ptr->Y =
                        pBuffer +
                        vaapi_mid->m_image.offsets[0];
                    ptr->U =
                        pBuffer +
                        vaapi_mid->m_image.offsets[1];
                    ptr->V = ptr->U + 1;
                } else
                    mfx_res = MFX_ERR_LOCK_MEMORY;
                break;
            case VA_FOURCC_YV12:
                if (vaapi_mid->m_fourcc == MFX_FOURCC_YV12) {
                    ptr->Pitch =
                        (mfxU16) vaapi_mid->
                        m_image.pitches[0];
                    ptr->Y =
                        pBuffer +
                        vaapi_mid->m_image.offsets[0];
                    ptr->V =
                        pBuffer +
                        vaapi_mid->m_image.offsets[1];
                    ptr->U =
                        pBuffer +
                        vaapi_mid->m_image.offsets[2];
                } else
                    mfx_res = MFX_ERR_LOCK_MEMORY;
                break;
            case VA_FOURCC_YUY2:
                if (vaapi_mid->m_fourcc == MFX_FOURCC_YUY2) {
                    ptr->Pitch =
                        (mfxU16) vaapi_mid->
                        m_image.pitches[0];
                    ptr->Y =
                        pBuffer +
                        vaapi_mid->m_image.offsets[0];
                    ptr->U = ptr->Y + 1;
                    ptr->V = ptr->Y + 3;
                } else
                    mfx_res = MFX_ERR_LOCK_MEMORY;
                break;
            case VA_FOURCC_RGBP:

                if (vaapi_mid->m_fourcc == MFX_FOURCC_RGBP) {
                    ptr->Pitch =
                        (mfxU16) vaapi_mid->
                        m_image.pitches[0];
                    ptr->B = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->G = pBuffer + vaapi_mid->m_image.offsets[1];
                    ptr->R = pBuffer + vaapi_mid->m_image.offsets[2];
                } else
                    mfx_res = MFX_ERR_LOCK_MEMORY;
                break;

            case VA_FOURCC_ARGB:
                if (vaapi_mid->m_fourcc == MFX_FOURCC_RGB4) {
                    ptr->Pitch =
                        (mfxU16) vaapi_mid->
                        m_image.pitches[0];
                    ptr->B =
                        pBuffer +
                        vaapi_mid->m_image.offsets[0];
                    ptr->G = ptr->B + 1;
                    ptr->R = ptr->B + 2;
                    ptr->A = ptr->B + 3;
                } else
                    mfx_res = MFX_ERR_LOCK_MEMORY;
                break;
            default:
                mfx_res = MFX_ERR_LOCK_MEMORY;
                break;
            }
        }
    }
    return mfx_res;
}

mfxStatus simple_unlock(mfxHDL pthis, mfxMemId mid, mfxFrameData* ptr)
{
    vaapiMemId* vaapi_mid = (vaapiMemId*) mid;

    if (!vaapi_mid || !(vaapi_mid->m_surface))
        return MFX_ERR_INVALID_HANDLE;

    if (MFX_FOURCC_P8 == vaapi_mid->m_fourcc) {     // bitstream processing
        vaUnmapBuffer(m_va_dpy, *(vaapi_mid->m_surface));
    } else {                // Image processing
        vaUnmapBuffer(m_va_dpy, vaapi_mid->m_image.buf);
        vaDestroyImage(m_va_dpy, vaapi_mid->m_image.image_id);

        if (NULL != ptr) {
            ptr->Pitch = 0;
            ptr->Y = NULL;
            ptr->U = NULL;
            ptr->V = NULL;
            ptr->A = NULL;
        }
    }
    return MFX_ERR_NONE;
}

mfxStatus simple_gethdl(mfxHDL pthis, mfxMemId mid, mfxHDL* handle)
{
    vaapiMemId* vaapi_mid = (vaapiMemId*) mid;

    if (!handle || !vaapi_mid || !(vaapi_mid->m_surface))
        return MFX_ERR_INVALID_HANDLE;

    *handle = vaapi_mid->m_surface; //VASurfaceID* <-> mfxHDL
    return MFX_ERR_NONE;
}

mfxStatus _simple_free(mfxHDL pthis, mfxFrameAllocResponse* response)
{
    vaapiMemId* vaapi_mids = NULL;
    VASurfaceID* surfaces = NULL;
    mfxU32 i = 0;
    bool isBitstreamMemory = false;
    bool actualFreeMemory = false;
    if (0 == memcmp(response, &(allocDecodeResponses[pthis].mfxResponse),
                    sizeof(*response))) {
        allocDecodeResponses[pthis].refCount--;
        if (0 == allocDecodeResponses[pthis].refCount)
            actualFreeMemory = true;
    }; // else actualFreeMemory = true;

    if (actualFreeMemory) {
        if (response->mids) {

            vaapi_mids = (vaapiMemId*) (response->mids[0]);

            isBitstreamMemory =
                (MFX_FOURCC_P8 ==
                 vaapi_mids->m_fourcc) ? true : false;
            surfaces = vaapi_mids->m_surface;
            for (i = 0; i < response->NumFrameActual; ++i) {
                if (MFX_FOURCC_P8 == vaapi_mids[i].m_fourcc)
                    vaDestroyBuffer(m_va_dpy, surfaces[i]);
                else if (vaapi_mids[i].m_sys_buffer)
                    free(vaapi_mids[i].m_sys_buffer);
            }

            free(vaapi_mids);
            free(response->mids);
            response->mids = NULL;

            if (!isBitstreamMemory)
                vaDestroySurfaces(m_va_dpy, surfaces, response->NumFrameActual);
            free(surfaces);
        }
        response->NumFrameActual = 0;
    }
    return MFX_ERR_NONE;
}

mfxStatus simple_free(mfxHDL pthis, mfxFrameAllocResponse* response)
{
    if (!response) return MFX_ERR_NULL_PTR;

    if (allocResponses.find(response->mids) == allocResponses.end()) {
        // Decode free response handling
        if (--allocDecodeResponses[pthis].refCount == 0) {
            _simple_free(pthis,response);
            allocDecodeResponses.erase(pthis);
        }
    } else {
        // Encode and VPP free response handling
        allocResponses.erase(response->mids);
        _simple_free(pthis,response);
    }

    return MFX_ERR_NONE;
}
