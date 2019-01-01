
#include <assert.h>
#include <iostream>
#include <limits>
#include <math.h>
#include <stdio.h>

#include "cm_rt.h"

struct surfaceInfoS{
	unsigned short height;
	unsigned short width;
	unsigned int fileSize;
	char* surfaceFormat;
	char* fileName;
	unsigned char inited;

    VASurfaceID    surfaceID;
	bool           bExternal;
};

CmSurface2D* setupSurface(CmDevice* &pCmDev, surfaceInfoS surfaceInfo);
CmSurface2D* setupOutputSurface(CmDevice* &pCmDev, surfaceInfoS surfaceInfo);
CmSurface2DUP* setupOutputSurfaceExt(CmDevice* &pCmDev, surfaceInfoS surfaceInfo, SurfaceIndex* &pSurfaceIndex, void *userptr);
void dumpOutput(CmEvent* pCmEvent, surfaceInfoS surfaceInfo, CmSurface2D* pCmSurface2D);
void dumpOutputBuffer(CmEvent* pCmEvent, surfaceInfoS surfaceInfo, CmBuffer* pCmSurfaceBuffer);
int setSamplerState(CmDevice* &pCmDev, SamplerIndex* &pInputSamplerIndex);
CmBuffer* setupBuffer(CmDevice* &pCmDev, surfaceInfoS surfaceInfo, SurfaceIndex* &pSurfaceIndex);
