
#include "SetupSurface.h"


CmSurface2D* pCmSurface2D_PA;

CmSurface2D* setupSurface(CmDevice* &pCmDev, surfaceInfoS surfaceInfo)
{
    //printf("Setting up input surfaces for %s ..\n",surfaceInfo.fileName);
    //printf("Width = %d\n",  surfaceInfo.width);	
    //printf("Height = %d\n", surfaceInfo.height);

    unsigned char *surfaceData = NULL;
    FILE * in;

    unsigned int result = 0;
    CM_SURFACE_FORMAT surfaceFormat;
    CmSurface2D* pCmSurface2D;
    pCmSurface2D = NULL;
    if(surfaceInfo.bExternal==TRUE)
    {
        result = pCmDev->CreateSurface2D(surfaceInfo.surfaceID, pCmSurface2D);
       if (result != CM_SUCCESS ) 
       { 
          printf("CM CreateSurface2D error: %d", result); exit(-1); 
       }
    }
    else
    {
        // Determine file size and Surface format
        if (strcmp(surfaceInfo.surfaceFormat,"ARGB") == 0) 
        {
            surfaceFormat = CM_SURFACE_FORMAT_A8R8G8B8;
            surfaceInfo.fileSize = surfaceInfo.width*surfaceInfo.height*4;
        }
        else if (strcmp(surfaceInfo.surfaceFormat,"R8") == 0)
        {
            surfaceFormat = CM_SURFACE_FORMAT_A8;
            surfaceInfo.fileSize = surfaceInfo.width*surfaceInfo.height;
        }
        else if (strcmp(surfaceInfo.surfaceFormat,"RAW") == 0)
        {
            surfaceFormat = CM_SURFACE_FORMAT_A8;
            surfaceInfo.fileSize = surfaceInfo.width*surfaceInfo.height;
        }
        else if (strcmp(surfaceInfo.surfaceFormat,"F32") == 0)
        {
            surfaceFormat = CM_SURFACE_FORMAT_R32F;
            surfaceInfo.fileSize = surfaceInfo.width*surfaceInfo.height * 4;
        }
        else if (strcmp(surfaceInfo.surfaceFormat,"R16") == 0)
        {
            surfaceFormat = CM_SURFACE_FORMAT_A8;
            surfaceInfo.fileSize = surfaceInfo.width*surfaceInfo.height * 2;
            surfaceInfo.width  = surfaceInfo.width * 2;
        }
        else if (strcmp(surfaceInfo.surfaceFormat,"NV12") == 0)
        {
            surfaceFormat = CM_SURFACE_FORMAT_NV12;
            surfaceInfo.fileSize = surfaceInfo.width*surfaceInfo.height * 3 / 2;
            surfaceInfo.width  = surfaceInfo.width;
        }
        else
        {
            printf("Input surface format, %s, not supported.", surfaceInfo.surfaceFormat); exit(-1);
        }

        if (surfaceInfo.inited == 1)
        {
            in = fopen(surfaceInfo.fileName, "rb"); 
            if (in == NULL) 
            { 
                perror("Unable to open surface input file."); exit(-1); 
            }

            surfaceData = (unsigned char*) malloc(surfaceInfo.fileSize);
            if(surfaceData == NULL)
            {
                fclose(in);
                return NULL;
            }
            if (fread(surfaceData, 1, surfaceInfo.fileSize, in) != surfaceInfo.fileSize) 
            { 
                perror("Surface file read failed."); exit(-1);  
            }

            fclose(in);

            if(surfaceData != NULL)
            {
                free(surfaceData);
                surfaceData = NULL;
            }
        }

        result = pCmDev->CreateSurface2D( surfaceInfo.width, surfaceInfo.height, surfaceFormat, pCmSurface2D);
        if (result != CM_SUCCESS ) 
        { 
        	printf("CM CreateSurface2D error: %d", result); exit(-1); 
        }
    }
    //pCmSurface2D->GetIndex(indexInput);

    return pCmSurface2D;
}
CmSurface2DUP* setupOutputSurfaceExt(CmDevice* &pCmDev, surfaceInfoS surfaceInfo, SurfaceIndex* &pSurfaceIndex, void *userptr)
{
    unsigned int result = 0;
    CmSurface2DUP *pCmSurface2D = nullptr;
    CmSurface2DUP *output_surface = nullptr;

    CM_SURFACE_FORMAT surfaceFormat;

    // Determine file size and Surface format
    if (strcmp(surfaceInfo.surfaceFormat,"ARGB") == 0) 
    {
        surfaceFormat = CM_SURFACE_FORMAT_A8R8G8B8;
        surfaceInfo.fileSize = surfaceInfo.width*surfaceInfo.height*4;
    }
    else if (strcmp(surfaceInfo.surfaceFormat,"AYUV") == 0) 
    {
        surfaceFormat = CM_SURFACE_FORMAT_A8R8G8B8;
        surfaceInfo.fileSize = surfaceInfo.width*surfaceInfo.height*4;
    }
    else if (strcmp(surfaceInfo.surfaceFormat,"R8") == 0)
    {
        surfaceFormat = CM_SURFACE_FORMAT_A8;
        surfaceInfo.fileSize = surfaceInfo.width*surfaceInfo.height;
    }
    else if (strcmp(surfaceInfo.surfaceFormat,"RAW") == 0)
    {
        surfaceFormat = CM_SURFACE_FORMAT_A8;
        surfaceInfo.fileSize = surfaceInfo.width*surfaceInfo.height;
    }
    else if (strcmp(surfaceInfo.surfaceFormat,"NV12") == 0)
    {
        surfaceFormat = CM_SURFACE_FORMAT_NV12;
        surfaceInfo.fileSize = surfaceInfo.width*surfaceInfo.height * 3 / 2;
    }
    else if (strcmp(surfaceInfo.surfaceFormat,"R16") == 0)
    {
        surfaceFormat = CM_SURFACE_FORMAT_A8;
        surfaceInfo.fileSize = surfaceInfo.width*surfaceInfo.height * 2;
        surfaceInfo.width = surfaceInfo.width * 2;
    }
    else
    {
        printf("Input surface format, %s, not supported.", surfaceInfo.surfaceFormat); exit(-1);
    }

    // Create CM surface
    result = pCmDev->CreateSurface2DUP( surfaceInfo.width, surfaceInfo.height, surfaceFormat,(void *)userptr , pCmSurface2D);
    if (result != CM_SUCCESS )
    { 
        printf("CM CreateSurface2D error: %d", result); 
         exit(-1); 
    }
    else{
         printf("create user ptr surface success\r\n");
    }
    // Create surface index	
   // pCmSurface2D->GetIndex(pSurfaceIndex);

    return pCmSurface2D;
}


CmSurface2D* setupOutputSurface(CmDevice* &pCmDev, surfaceInfoS surfaceInfo)
{
	unsigned int result = 0;
	CmSurface2D* pCmSurface2D = NULL;

	CM_SURFACE_FORMAT surfaceFormat;

	// Determine file size and Surface format
	if (strcmp(surfaceInfo.surfaceFormat,"ARGB") == 0) 
	{
		surfaceFormat = CM_SURFACE_FORMAT_A8R8G8B8;
		surfaceInfo.fileSize = surfaceInfo.width*surfaceInfo.height*4;
	}
	else if (strcmp(surfaceInfo.surfaceFormat,"AYUV") == 0) 
	{
		surfaceFormat = CM_SURFACE_FORMAT_A8R8G8B8;
		surfaceInfo.fileSize = surfaceInfo.width*surfaceInfo.height*4;
	}
	else if (strcmp(surfaceInfo.surfaceFormat,"R8") == 0)
	{
		surfaceFormat = CM_SURFACE_FORMAT_A8;
		surfaceInfo.fileSize = surfaceInfo.width*surfaceInfo.height;
	}
	else if (strcmp(surfaceInfo.surfaceFormat,"RAW") == 0)
	{
		surfaceFormat = CM_SURFACE_FORMAT_A8;
		surfaceInfo.fileSize = surfaceInfo.width*surfaceInfo.height;
	}
	else if (strcmp(surfaceInfo.surfaceFormat,"NV12") == 0)
	{
		surfaceFormat = CM_SURFACE_FORMAT_NV12;
		surfaceInfo.fileSize = surfaceInfo.width*surfaceInfo.height * 3 / 2;
	}
	else if (strcmp(surfaceInfo.surfaceFormat,"R16") == 0)
	{
		surfaceFormat = CM_SURFACE_FORMAT_A8;
		surfaceInfo.fileSize = surfaceInfo.width*surfaceInfo.height * 2;
		surfaceInfo.width = surfaceInfo.width * 2;
	}
	else
	{
		printf("Input surface format, %s, not supported.", surfaceInfo.surfaceFormat); exit(-1);
	}

	// Create CM surface
	result = pCmDev->CreateSurface2D( surfaceInfo.width, surfaceInfo.height, surfaceFormat, pCmSurface2D);
	if (result != CM_SUCCESS ) { 
		printf("CM CreateSurface2D error: %d", result); exit(-1); }
	// Create surface index	
	//pCmSurface2D->GetIndex(pSurfaceIndex);

	return pCmSurface2D;
}

CmBuffer* setupBuffer(CmDevice* &pCmDev, surfaceInfoS surfaceInfo, SurfaceIndex* &pSurfaceIndex)
{
	unsigned int result = 0;
	CmBuffer* pCmBuffer = NULL;
	unsigned char *surfaceData;
	FILE * in;

	// Create CM surface
	result = pCmDev->CreateBuffer( surfaceInfo.width * sizeof(int), pCmBuffer);
	if (result != CM_SUCCESS ) { 
		printf("CM CreateSurface2D error: %d", result); exit(-1); }

	surfaceInfo.fileSize = surfaceInfo.width * sizeof(int);

	if (surfaceInfo.inited == 1)
	{
		in = fopen(surfaceInfo.fileName, "rb"); 
		if (in == NULL) 
		{ 
			perror("Unable to open surface input file."); exit(-1); 
		}

		surfaceData = (unsigned char*) malloc(surfaceInfo.fileSize);
        if (surfaceData == NULL)
        {
            fclose(in);
            return NULL;
        }
		if (fread(surfaceData, 1, surfaceInfo.fileSize, in) != surfaceInfo.fileSize) 
		{ 
			perror("Surface file read failed."); exit(-1);  
		}
		fclose(in);		
	}
	else
	{
			surfaceData = (unsigned char*) malloc(surfaceInfo.fileSize);
			memset(surfaceData, 0, surfaceInfo.fileSize);
	}

	result = pCmBuffer->WriteSurface(surfaceData, NULL);
	if (result != CM_SUCCESS )
	{ 
		printf("CM WriteSurface error: %d", result); exit(-1); 
	}

	// Create surface index	
	pCmBuffer->GetIndex(pSurfaceIndex);

    if (surfaceData)
        free(surfaceData);

	return pCmBuffer;
}

void dumpOutput(CmEvent* pCmEvent, surfaceInfoS surfaceInfo, CmSurface2D* pCmSurface2D){
	
	int result;

	CM_SURFACE_FORMAT surfaceFormat;

	// Determine file size and Surface format
	if (strcmp(surfaceInfo.surfaceFormat,"ARGB") == 0) 
	{
		surfaceFormat = CM_SURFACE_FORMAT_A8R8G8B8;
		surfaceInfo.fileSize = surfaceInfo.width*surfaceInfo.height*4;
	}
	else if (strcmp(surfaceInfo.surfaceFormat,"AYUV") == 0) 
	{
		surfaceFormat = CM_SURFACE_FORMAT_A8B8G8R8;
		surfaceInfo.fileSize = surfaceInfo.width*surfaceInfo.height*4;
	}
	else if (strcmp(surfaceInfo.surfaceFormat,"R8") == 0)
	{
		surfaceFormat = CM_SURFACE_FORMAT_A8;
		surfaceInfo.fileSize = surfaceInfo.width*surfaceInfo.height;
	}
	else if (strcmp(surfaceInfo.surfaceFormat,"RAW") == 0)
	{
		surfaceFormat = CM_SURFACE_FORMAT_A8;
		surfaceInfo.fileSize = surfaceInfo.width*surfaceInfo.height;
	}
	else if (strcmp(surfaceInfo.surfaceFormat,"NV12") == 0)
	{
		surfaceFormat = CM_SURFACE_FORMAT_NV12;
		surfaceInfo.fileSize = surfaceInfo.width*surfaceInfo.height * 3 / 2;
        printf("trying to dump NV12 surface\r\n");
	}
	else if (strcmp(surfaceInfo.surfaceFormat,"R16") == 0)
	{
		surfaceFormat = CM_SURFACE_FORMAT_L16;
		surfaceInfo.fileSize = surfaceInfo.width*surfaceInfo.height * 2;
	}
	else
	{
		printf("Input surface format, %s, not supported.", surfaceInfo.surfaceFormat); exit(-1);
	}

	// Read Surface
	unsigned char* data;
	data = (unsigned char*) malloc(surfaceInfo.fileSize);
    if (data == NULL)
    {
        perror("error");
        exit(-1);
    }

    result = pCmSurface2D->ReadSurface( data, pCmEvent );
	if (result != CM_SUCCESS ) 
	{ 
		printf("CM ReadSurface error: %d",result); exit(-1); 
	}

    printf("dump file %s\r\n",surfaceInfo.fileName);
	// Dump output
	FILE * outFile;
    outFile = fopen(surfaceInfo.fileName, "a+b");
	if (outFile == NULL) 
	{ 
		perror("Error: Writing to outputimage"); 
		exit(-1); 
	}

	if (fwrite(data, 1, surfaceInfo.fileSize, outFile) !=  surfaceInfo.fileSize) 
	{ 
		perror("error"); 
		exit(-1); 
	}

	fclose(outFile);
}

void dumpOutputBuffer(CmEvent* pCmEvent, surfaceInfoS surfaceInfo, CmBuffer* pCmSurfaceBuffer){
    
    int result;

    // Read Surface
    unsigned char* data;
    surfaceInfo.fileSize = surfaceInfo.width * sizeof(int);

    data = (unsigned char*) malloc(surfaceInfo.fileSize);
    if (data == NULL)
    {
        perror("error");
        exit(-1);
    }

    result = pCmSurfaceBuffer->ReadSurface( data, pCmEvent );
    if (result != CM_SUCCESS ) 
    { 
        printf("CM ReadSurface error: %d",result); exit(-1); 
    }

    // Dump output
    FILE * outFile;
    outFile = fopen(surfaceInfo.fileName, "wb");
    if (outFile == NULL) 
    { 
        perror("Error: Writing to outputimage"); 
        exit(-1); 
    }

    if (fwrite(data, 1, surfaceInfo.fileSize, outFile) !=  surfaceInfo.fileSize) 
    { 
        perror("error"); 
        exit(-1); 
    }

    fclose(outFile);
}

int setSamplerState(CmDevice* &pCmDev, SamplerIndex* &pInputSamplerIndex)
{
	int result;
    CmSampler* pSampler = NULL;
	CM_SAMPLER_STATE samplerState;
	samplerState.magFilterType = CM_TEXTURE_FILTER_TYPE_LINEAR;
	samplerState.minFilterType = CM_TEXTURE_FILTER_TYPE_LINEAR;
	samplerState.addressU      = CM_TEXTURE_ADDRESS_CLAMP;
	samplerState.addressV      = CM_TEXTURE_ADDRESS_CLAMP;
	samplerState.addressW      = CM_TEXTURE_ADDRESS_CLAMP;

	result = pCmDev->CreateSampler(samplerState, pSampler);
	if (result != CM_SUCCESS ) {
		printf("CM CreateSampler error");
		return result;
	}

	pSampler->GetIndex(pInputSamplerIndex);

    return 0;
}
