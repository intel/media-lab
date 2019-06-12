/*
// Copyright (c) 2018 Intel Corporation
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

#pragma once

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <fcntl.h>
#include <new>
#include <stdlib.h>
#include <assert.h>

#include "mfxvideo++.h"

#include "va/va.h"
#include "va/va_drm.h"
#include "mfxvideo++.h"
#include "mfxjpeg.h"

// =================================================================
// VAAPI functionality required to manage VA surfaces
mfxStatus CreateVAEnvDRM(mfxHDL* displayHandle);

// utility
mfxStatus va_to_mfx_status(VAStatus va_res);

typedef timespec mfxTime;


// =================================================================
// Helper macro definitions...
#define MSDK_PRINT_RET_MSG(ERR)         {PrintErrString(ERR, __FILE__, __LINE__);}
#define MSDK_CHECK_RESULT(P, X, ERR)    {if ((X) > (P)) {MSDK_PRINT_RET_MSG(ERR); return ERR;}}
#define MSDK_CHECK_POINTER(P, ERR)      {if (!(P)) {MSDK_PRINT_RET_MSG(ERR); return ERR;}}
#define MSDK_CHECK_ERROR(P, X, ERR)     {if ((X) == (P)) {MSDK_PRINT_RET_MSG(ERR); return ERR;}}
#define MSDK_IGNORE_MFX_STS(P, X)       {if ((X) == (P)) {P = MFX_ERR_NONE;}}
#define MSDK_BREAK_ON_ERROR(P)          {if (MFX_ERR_NONE != (P)) break;}
#define MSDK_SAFE_DELETE_ARRAY(P)       {if (P) {delete[] P; P = NULL;}}
#define MSDK_ALIGN32(X)                 (((mfxU32)((X)+31)) & (~ (mfxU32)31))
#define MSDK_ALIGN16(value)             (((value + 15) >> 4) << 4)
#define MSDK_SAFE_RELEASE(X)            {if (X) { X->Release(); X = NULL; }}
#define MSDK_MAX(A, B)                  (((A) > (B)) ? (A) : (B))

// Usage of the following two macros are only required for certain Windows DirectX11 use cases
#define WILL_READ  0x1000
#define WILL_WRITE 0x2000

// =================================================================
// Intel Media SDK memory allocator entrypoints....
// Implementation of this functions is OS/Memory type specific.
mfxStatus simple_alloc(mfxHDL pthis, mfxFrameAllocRequest* request, mfxFrameAllocResponse* response);
mfxStatus simple_lock(mfxHDL pthis, mfxMemId mid, mfxFrameData* ptr);
mfxStatus simple_unlock(mfxHDL pthis, mfxMemId mid, mfxFrameData* ptr);
mfxStatus simple_gethdl(mfxHDL pthis, mfxMemId mid, mfxHDL* handle);
mfxStatus simple_free(mfxHDL pthis, mfxFrameAllocResponse* response);



// =================================================================
// Utility functions, not directly tied to Media SDK functionality
//

void PrintErrString(int err,const char* filestr,int line);

// Read bit stream data from file. Stream is read as large chunks (= many frames)
mfxStatus ReadBitStreamData(mfxBitstream* pBS, FILE* fSource);

// Get free raw frame surface
int GetFreeSurfaceIndex(mfxFrameSurface1** pSurfacesPool, mfxU16 nPoolSize);

// Initialize Intel Media SDK Session, device/display and memory manager
mfxStatus Initialize(mfxIMPL impl, mfxVersion ver, MFXVideoSession* pSession, mfxFrameAllocator* pmfxAllocator, bool bCreateSharedHandles = false);

