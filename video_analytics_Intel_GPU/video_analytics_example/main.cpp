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

/*
// brief The entry point for the Inference Engine object_detection sample application
*/

#include "main.h"
#include <chrono>
#include <thread>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <vector>
#include <queue>
#include <signal.h>

#include <pthread.h>  
#include <unistd.h>  
#include <semaphore.h>
#include <chrono>

// =================================================================
// Intel Media SDK
// =================================================================
#include <mfxvideo++.h>
#include <mfxstructures.h>
#include <unistd.h>
#include "common.h"
#include "dualpipe.h"


// =================================================================
// Inference Engine Interface
// =================================================================

#include "detector.hpp"


// =================================================================
// KCF tracker Interface
// =================================================================

#include "kcftracker.hpp"


using namespace cv;
using namespace std;

//#define TEST_KCF_TRACK_WITH_GPU 
#define NUM_OF_CHANNELS 48   // Maximum supported decoding and video processing 

// Inference Device 
#define NUM_OF_GPU_INFER       1// two GPU context
//#define ENABLE_WORKLOAD_BALANCE 
#define NUM_OF_CPU_BATCH       2

// Helper macro definitions
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
#define MSDK_MIN(A, B)                  (((A) < (B)) ? (A) : (B))

#define MFX_MAKEFOURCC(A,B,C,D)    ((((int)A))+(((int)B)<<8)+(((int)C)<<16)+(((int)D)<<24))

#ifndef MFX_FOURCC_RGBP
#define MFX_FOURCC_RGBP  MFX_MAKEFOURCC('R','G','B','P')
#endif


Detector gDetector[NUM_OF_GPU_INFER];
sem_t gNewtaskAvaiable;

#ifdef TEST_KCF_TRACK_WITH_GPU
sem_t gNewTrackTaskAvaiable[NUM_OF_CHANNELS];
sem_t gDetectResultAvaiable[NUM_OF_CHANNELS];
#endif

sem_t             gsemtInfer[NUM_OF_GPU_INFER];
queue<infer_task_t> gtaskque[NUM_OF_GPU_INFER];
pthread_mutex_t   mutexinfer[NUM_OF_GPU_INFER]; 

// Display
#ifdef TEST_KCF_TRACK_WITH_GPU
#define INPUTNUM 1
queue<Detector::DetctorResult > gDetectResultque[NUM_OF_CHANNELS]; // Queue of the inference output
#else
#define INPUTNUM 6
#endif
pthread_mutex_t mutexshow ;    // Mutex of the dislay queue
sem_t           g_semtshow;    // Notify the display thread to show                     
queue<vector<Detector::DetctorResult> > gresultque; // Queue of the inference output
queue<vector<Detector::DetctorResult> > gresultque1; // Queue of the inference output

struct DecThreadConfig
{
public:
    DecThreadConfig():
        totalDecNum(0),
        f_i(NULL),
        pmfxDEC(NULL),
        nDecSurfNum(0),
        nVPP_In_SurfNum(0),
        nVPP_Out_SurfNum(0),
        pmfxBS(NULL),
        pmfxDecSurfaces(NULL),
        pmfxVPP_In_Surfaces(NULL),
        pmfxVPP_Out_Surfaces(NULL),
        pmfxSession(NULL),
        pmfxAllocator(NULL),
        pmfxVPP(NULL),
        decOutputFile(0),
        nChannel(0),
        nFPS(0),
        bStartCount(false),
        nFrameProcessed(0)
    {
    };
    int totalDecNum;
    FILE *f_i;
    MFXVideoDECODE *pmfxDEC;
    mfxU16 nDecSurfNum;
    mfxU16 nVPP_In_SurfNum;
    mfxU16 nVPP_Out_SurfNum;
    mfxBitstream *pmfxBS;
    mfxFrameSurface1** pmfxDecSurfaces;
    mfxFrameSurface1** pmfxVPP_In_Surfaces;
    mfxFrameSurface1** pmfxVPP_Out_Surfaces;
    MFXVideoSession *  pmfxSession;
    mfxFrameAllocator *pmfxAllocator;
    MFXVideoVPP       *pmfxVPP;

    int decOutputFile;
    int nChannel;
    int nFPS;
    bool bStartCount;
    int nFrameProcessed;
    VaDualPipe *dpipe;
    VaDualPipe *dKCFpipe; // Pipe between decoding and track thread

    std::chrono::high_resolution_clock::time_point tmStart;
    std::chrono::high_resolution_clock::time_point tmEnd;

};


struct TrackerThreadConfig
{
public:
    TrackerThreadConfig():
        totalDecNum(0),
        pmfxDEC(NULL),
        nDecSurfNum(0),
        nVPP_In_SurfNum(0),
        nVPP_Out_SurfNum(0),
        pmfxBS(NULL),
        pmfxDecSurfaces(NULL),
        pmfxVPP_In_Surfaces(NULL),
        pmfxVPP_Out_Surfaces(NULL),
        pmfxSession(NULL),
        pmfxAllocator(NULL),
        pmfxVPP(NULL),
        width(0),
        height(0),
        nChannel(0)
    {
    };
    int totalDecNum;
    MFXVideoDECODE     *pmfxDEC;
    mfxU16             nDecSurfNum;
    mfxU16             nVPP_In_SurfNum;
    mfxU16             nVPP_Out_SurfNum;
    mfxBitstream *pmfxBS;
    mfxFrameSurface1** pmfxDecSurfaces;
    mfxFrameSurface1** pmfxVPP_In_Surfaces;
    mfxFrameSurface1** pmfxVPP_Out_Surfaces;
    MFXVideoSession *  pmfxSession;
    mfxFrameAllocator *pmfxAllocator;
    MFXVideoVPP       *pmfxVPP;
    int width;
    int height;
    int nChannel;
    VaDualPipe *dpipe;
};

std::vector<DecThreadConfig *>   vpDecThradConfig;
std::vector<TrackerThreadConfig *>   vpTrackerThreadConfig;


int grunning = true;
//Infernece information
size_t   g_batchSize;
int gNet_input_width;
int gNet_input_height;
int total_frame[3] ={0};
int each_frame[INPUTNUM];
static string  CLASSES[] = {"background",
           "face", "!bicycle", "bird", "boat",
           "!bottle", "!bus", "car", "cat", "!chair",
           "cow", "!diningtable", "dog", "horse",
           "motorbike", "person", "!pottedplant",
           "sheep", "sofa", "!train", "!tvmonitor"};
// Performance information
int total_fps;

void App_ShowUsage(void);
mfxStatus WriteRawFrameToMemory(mfxFrameSurface1* pSurface, int dest_w, int dest_h, unsigned char* dest, mfxU32 fourCC);
cv::Mat createMat(unsigned char *rawData, unsigned int dimX, unsigned int dimY);

static std::string fileNameNoExt(const std::string &filepath) {
    auto pos = filepath.rfind('.');
    if (pos == std::string::npos) return filepath;
    return filepath.substr(0, pos);
}

// handle to end the process
void sigint_hanlder(int s)
{
    grunning = false;
}

// ================= Display Thread =======
// in current version, each grids with the same size (wxh)
struct DisplayThreadConfig
{
public:
    DisplayThreadConfig():
        nCellWidth(0),
        nCellHeight(0),
        nRows(0),
        nCols(0)
    {
    };

    int nCellWidth;  // width of the each display grid
    int nCellHeight; // Height of each display grid
    int nRows;       // # of rows in each display grids
    int nCols;       // # of grids in each row

};

//Thread to collect performance information
void *thr_fps(void *arg)
{
    typedef std::chrono::high_resolution_clock Time;
    typedef std::chrono::duration<double, std::ratio<1, 1000>> ms;
    typedef std::chrono::duration<float> fsec;	

    usleep(1000*1000);//1000ms to fullfill buffer
    memset(total_frame,0, sizeof(unsigned int)*NUM_OF_GPU_INFER);

    auto t0=Time::now(),t1=Time::now();
    while(grunning){
        usleep(1000*1000*2);//2s
        t1 = Time::now();
        fsec fs = t1 - t0;
        double timeUsed = std::chrono::duration_cast<ms>(fs).count();
        if(FLAGS_pi && (FLAGS_infer == 1)){
            int nInferedFrame = 0;
            for(int nIndex=0; nIndex<NUM_OF_GPU_INFER; nIndex++){
                nInferedFrame +=total_frame[nIndex];
            }
            total_fps = nInferedFrame*1000/timeUsed;
            std::cout << "RealTime fps=" << total_fps << " Total frame: "<<nInferedFrame<< "\n" << std::endl;
            memset(total_frame,0, sizeof(unsigned int)*NUM_OF_GPU_INFER);
        }
        if(FLAGS_pd){
            int nTotalDecFrames = 0;
            for (auto& pDecThrConf : vpDecThradConfig) {
                nTotalDecFrames += pDecThrConf->nFrameProcessed;
                pDecThrConf->nFrameProcessed = 0;
            }
            total_fps = nTotalDecFrames*1000/timeUsed;
            std::cout << "Decode fps=" << total_fps  << "(f/s)\n" << std::endl;
        }
        t0 = Time::now();
    }
    std::cout<<"Performance thread is done"<<std::endl;
    return NULL;
}

// ================= Dispay Thread =======
void *DisplayThreadFunc(void *arg)
{
    struct DisplayThreadConfig *pDisplayConfig = NULL;
    int nCellWidth  = 0;
    int nCellHeight = 0;
    int nCols   = 0;
    int nRows   = 0;
    int prev_fps=0;

    if( NULL == arg ){
        std::cout<<"Invalid Display configuration"<<std::endl;
        return NULL;
    }

    pDisplayConfig= (DisplayThreadConfig *) arg;
    nCellWidth  = pDisplayConfig->nCellWidth;
    nCellHeight = pDisplayConfig->nCellHeight;
    nCols       = pDisplayConfig->nCols;
    nRows       = pDisplayConfig->nRows;
    
    int fpshaswrite[INPUTNUM];
    cv::Mat onescreen(nCellHeight*nRows, nCellWidth*nCols, CV_8UC3);
    cv::Mat eachscreen[INPUTNUM];
    int eachscreen_display_order[INPUTNUM]={0};
    for(int i=0;i<INPUTNUM;i++){
        eachscreen[i]=onescreen(cv::Rect(nCellWidth*(i%nCols),(i>nRows)?nCellHeight:0,nCellWidth,nCellHeight));
    }
    while(grunning)
    {
        sem_wait(&g_semtshow);
        vector<Detector::DetctorResult> objects;
        if(!gresultque.empty()){
            pthread_mutex_lock(&mutexshow); 	
            objects = gresultque.front();	
            gresultque.pop();			
            pthread_mutex_unlock(&mutexshow); 	
            memset(fpshaswrite,0,sizeof(fpshaswrite));

            for(int k=0;k<objects.size();k++){
                for(int i=0;i<objects[k].boxs.size();i++)
                {
             
                    if(CLASSES[(int)(objects[k].boxs[i].classid)][0]=='!')
                        continue;
               
                    cv::rectangle(objects[k].orgimg,cvPoint(objects[k].boxs[i].left,objects[k].boxs[i].top),cvPoint(objects[k].boxs[i].right,objects[k].boxs[i].bottom),cv::Scalar(71, 99, 250),2);
                    std::stringstream ss;  
                    ss << CLASSES[(int)(objects[k].boxs[i].classid)] << "/" << objects[k].boxs[i].confidence;  
                    std::string  text = ss.str();  
                    cv::putText(objects[k].orgimg, text, cvPoint(objects[k].boxs[i].left,objects[k].boxs[i].top+20), cv::FONT_HERSHEY_PLAIN, 1.0f, cv::Scalar(0, 255, 255));  	
                }
                {
                    if(fpshaswrite[objects[k].inputid]==0)
                    {
                        //if(objects[k].frameno > eachscreen_display_order[objects[k].inputid]){
                            objects[k].orgimg.copyTo(eachscreen[objects[k].inputid]);
                            eachscreen_display_order[objects[k].inputid] = objects[k].frameno;                         
                       // }
                        fpshaswrite[objects[k].inputid]=1;
                    } 	
                }
            }
        #ifndef TEST_KCF_TRACK_WITH_GPU
            if(!gresultque1.empty()){
                Detector::DetctorResult tempObj;
                pthread_mutex_lock(&mutexshow); 	
                objects = gresultque1.front();
                static unsigned int dispNum =0;	
                if(dispNum%NUM_OF_CPU_BATCH == 1){
                   gresultque1.pop();
                }
                
                tempObj = objects[dispNum%NUM_OF_CPU_BATCH];		
                dispNum++;
                pthread_mutex_unlock(&mutexshow); 

                for(int i=0;i<tempObj.boxs.size();i++)
                {
                    if(CLASSES[(int)(tempObj.boxs[i].classid)][0]=='!')
                        continue;
                    cv::rectangle(tempObj.orgimg,cvPoint(tempObj.boxs[i].left,tempObj.boxs[i].top),cvPoint(tempObj.boxs[i].right,tempObj.boxs[i].bottom),cv::Scalar(71, 99, 250),2);
                    std::stringstream ss;  
                    ss << CLASSES[(int)(tempObj.boxs[i].classid)] << "/" << tempObj.boxs[i].confidence;  
                    std::string  text = ss.str();  
                    cv::putText(tempObj.orgimg, text, cvPoint(tempObj.boxs[i].left,tempObj.boxs[i].top+20), cv::FONT_HERSHEY_PLAIN, 1.0f, cv::Scalar(0, 255, 255));  	
                }//for(int i=0;i<objects[k].boxs.size();i++)

                tempObj.orgimg.copyTo(eachscreen[tempObj.inputid]);
               // }//for(int k=0;k<objects.size();k++){
            }// if(!gresultque1.empty()){	
            // Update Peformance at the last display Grid
            if(prev_fps != total_fps)
            {
                std::stringstream ss;
                cv::Mat frame(nCellWidth,nCellHeight, CV_8UC3);
                memset(frame.data, 0, nCellWidth*nCellHeight*3);
                ss << "Total FPS: " << total_fps <<" (f/s)"; 
                prev_fps = total_fps;
                std::string  text = ss.str();  				
                cv::putText(frame, text, cvPoint(0,20), cv::FONT_HERSHEY_PLAIN, 1.0f, cv::Scalar(127, 255, 0));  
                frame.copyTo(eachscreen[INPUTNUM-1]);		
            }
       #endif
            cv::namedWindow( "Display window", cv::WINDOW_AUTOSIZE );
            cv::imshow( "Display window", onescreen );  
            cv::waitKey(33);
    
        } // wait for display		
    }//while(grunnig)
    std::cout<<"Dispaly Thread is done"<<std::endl;
    return NULL;
}



// ================= Decoding Thread =======
void *DecodeThreadFunc(void *arg)
{
    struct DecThreadConfig *pDecConfig = NULL;
    int nFrame = 0;
    mfxBitstream mfxBS;
    mfxStatus sts = MFX_ERR_NONE;
    mfxSyncPoint syncpDec;
    mfxSyncPoint syncpVPP;

    struct timeval pipeStartTs = {0};

    int nIndexDec     = 0;
    int nIndexVPP_In  = 0;
    int nIndexVPP_In2 = 0;
    int nIndexVPP_Out = 0;
    bool bNeedMore    = false;

    unsigned char *temp_img_buffer = (unsigned char *)malloc(gNet_input_width*gNet_input_height*4);
    if (temp_img_buffer == NULL)
        return NULL;
    pDecConfig= (DecThreadConfig *) arg;
    if( NULL == pDecConfig)
    {
        std::cout << std::endl << "Failed Decode Thread Configuration" << std::endl;
        if(temp_img_buffer)
        {
            free(temp_img_buffer);
        }
        return NULL;
    }
    std::cout << std::endl <<">channel("<<pDecConfig->nChannel<<") Initialized " << std::endl;

    pDecConfig->tmStart = std::chrono::high_resolution_clock::now();
    while ((MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts || MFX_ERR_MORE_SURFACE == sts))
    {
       // std::cout << std::endl <<">channel("<<pDecConfig->nChannel<<") Dec: nFrame:"<<nFrame<<" totalDecNum:"<<pDecConfig->totalDecNum << std::endl;
        if (grunning == false) { break;};
        if (MFX_WRN_DEVICE_BUSY == sts)
        {
            usleep(1000); // Wait if device is busy, then repeat the same call to DecodeFrameAsync
        }

        if (MFX_ERR_MORE_DATA == sts)
        {   
            sts = ReadBitStreamData(pDecConfig->pmfxBS, pDecConfig->f_i); // Read more data into input bit stream
            if(sts != 0){
              fseek(pDecConfig->f_i,0, SEEK_SET);
              sts = ReadBitStreamData(pDecConfig->pmfxBS, pDecConfig->f_i);
            }
            MSDK_BREAK_ON_ERROR(sts);
        }
        if (FLAGS_pl)
        {
            gettimeofday(&pipeStartTs, NULL);
        }
        
        if (MFX_ERR_MORE_SURFACE == sts || MFX_ERR_NONE == sts)
        {
recheck21:

            nIndexDec =GetFreeSurfaceIndex(pDecConfig->pmfxDecSurfaces, pDecConfig->nDecSurfNum);
            if(nIndexDec == MFX_ERR_NOT_FOUND){
               //std::cout << std::endl<< ">channel("<<pDecConfig->nChannel<<") >> Not able to find an avaialbe decoding recon surface" << std::endl;
               usleep(10000);
               goto recheck21;
            }
        }

        if(bNeedMore == false)
        {
            if (MFX_ERR_MORE_SURFACE == sts || MFX_ERR_NONE == sts)
            {   
recheck:
                nIndexVPP_In = GetFreeSurfaceIndex(pDecConfig->pmfxVPP_In_Surfaces, pDecConfig->nVPP_In_SurfNum); // Find free frame surface
         
                if(nIndexVPP_In == MFX_ERR_NOT_FOUND){
                   std::cout << ">channel("<<pDecConfig->nChannel<<") >> Not able to find an avaialbe VPP input surface" << std::endl;
                     
                   goto recheck;
                }
            }
        }

        // Decode a frame asychronously (returns immediately)
        //  - If input bitstream contains multiple frames DecodeFrameAsync will start decoding multiple frames, and remove them from bitstream
        sts = pDecConfig->pmfxDEC->DecodeFrameAsync(pDecConfig->pmfxBS, pDecConfig->pmfxDecSurfaces[nIndexDec], &(pDecConfig->pmfxVPP_In_Surfaces[nIndexVPP_In]), &syncpDec);

        // Ignore warnings if output is available,eat the Decode surface
        // if no output and no action required just repecodeFrameAsync call
        if (MFX_ERR_NONE < sts && syncpDec)
        {
            bNeedMore = false;
            sts = MFX_ERR_NONE;
        }
        else if(MFX_ERR_MORE_DATA == sts)
        {
            bNeedMore = true;
        }
        else if(MFX_ERR_MORE_SURFACE == sts)
        {
            bNeedMore = true;
        }
        //else

        if (MFX_ERR_NONE == sts )
        {
            pDecConfig->nFrameProcessed ++;

          
#ifdef TEST_KCF_TRACK_WITH_GPU		
		    vsource_frame_t *srcTrackFrame = NULL;
            srcTrackFrame = (vsource_frame_t*)pDecConfig->dKCFpipe->Get();
            if(srcTrackFrame == NULL)
			    break;
			if ((nFrame % 6) == 0) 
			{
				//std::cout<<"it is a Key frame, needs to do detection "<<nFrame<<std::endl;
			    srcTrackFrame->bROIRrefresh = true;
			}else{
			    //std::cout<<"Not a key frame, just do a tracking "<<nFrame<<std::endl;
				srcTrackFrame->bROIRrefresh = false;
			}

		    mfxFrameSurface1* pSurface = pDecConfig->pmfxVPP_In_Surfaces[nIndexVPP_In];
			pSurface->Data.Locked +=1;				 
            srcTrackFrame->pmfxSurface = pSurface;
			//std::cout<<"	  Push frame into tracking thread: "<<pSurface<<std::endl; 
			srcTrackFrame->timestamp = pipeStartTs;

            pDecConfig->dKCFpipe->Store(srcTrackFrame);
           
            sem_post(&gNewTrackTaskAvaiable[pDecConfig->nChannel]);
#endif

#ifdef TEST_KCF_TRACK_WITH_GPU	
            if ((nFrame % 6) == 0)
#endif
            {
               //std::cout <<" send to inference workload" << std::endl;
#ifdef TEST_KCF_TRACK_WITH_GPU				   
               usleep(30000); 
#endif
recheck2:
               nIndexVPP_Out = GetFreeSurfaceIndex(pDecConfig->pmfxVPP_Out_Surfaces, pDecConfig->nVPP_Out_SurfNum); // Find free frame surface

               if(nIndexVPP_Out == MFX_ERR_NOT_FOUND){
                   std::cout << ">channel("<<pDecConfig->nChannel<<") >> Not able to find an avaialbe VPP output surface" << std::endl;
                 //  return NULL;
                   goto recheck2;
               }
                              
               for (;;)
               {
                   // Process a frame asychronously (returns immediately) 
                   sts = pDecConfig->pmfxVPP->RunFrameVPPAsync(pDecConfig->pmfxVPP_In_Surfaces[nIndexVPP_In], pDecConfig->pmfxVPP_Out_Surfaces[nIndexVPP_Out], NULL, &syncpVPP);

                   if (MFX_ERR_NONE < sts && !syncpVPP) // repeat the call if warning and no output
                   {
                       if (MFX_WRN_DEVICE_BUSY == sts)
                       {
                           std::cout << std::endl << "> warning: MFX_WRN_DEVICE_BUSY" << std::endl;
                           usleep(1000); // wait if device is busy
                       }
                   }
                   else if (MFX_ERR_NONE < sts && syncpVPP)
                   {
                       sts = MFX_ERR_NONE; // ignore warnings if output is available
                       break;
                   }
                   else{
                       break; // not a warning
                   }
               }

               // VPP needs more data, let decoder decode another frame as input
               if (MFX_ERR_MORE_DATA == sts)
                 {
                     continue;
                 }
                 else if (MFX_ERR_MORE_SURFACE == sts)
                 {
                    // Not relevant for the illustrated workload! Therefore not handled.
                    // Relevant for cases when VPP produces more frames at output than consumes at input. E.g. framerate conversion 30 fps -> 60 fps
                    break;
                 }
                 //else
                 // std::cout << ">> Vpp sts" << sts << std::endl;
                 // MSDK_BREAK_ON_ERROR(sts);
                
  
                 if (MFX_ERR_NONE == sts)
                 { 
                    sts = pDecConfig->pmfxSession->SyncOperation(syncpVPP, 60000); // Synchronize. Wait until decoded frame is ready
                    if(FLAGS_infer == 0){
                        continue;
                    }
                    vsource_frame_t *srcframe = NULL;
                    srcframe = (vsource_frame_t*) pDecConfig->dpipe->Get();
                    srcframe->channel  = pDecConfig->nChannel;
                    srcframe->frameno  = pDecConfig->nFrameProcessed;
                    mfxU32 i, j, h, w;

                    mfxFrameSurface1* pSurface = pDecConfig->pmfxVPP_Out_Surfaces[nIndexVPP_Out];

                    pDecConfig->pmfxAllocator->Lock(pDecConfig->pmfxAllocator->pthis, 
                                                      pSurface->Data.MemId, 
                                                      &(pSurface->Data));
                    mfxFrameInfo* pInfo = &pSurface->Info;
                    mfxFrameData* pData = &pSurface->Data;
                           
                  #ifndef TEST_KCF_TRACK_WITH_GPU	

                    mfxU8* ptr;
                    if (pInfo->CropH > 0 && pInfo->CropW > 0)
                    {
                        w = pInfo->CropW;
                        h = pInfo->CropH;
                    }
                    else
                    {
                        w = pInfo->Width;
                        h = pInfo->Height;
                    }

                    mfxU8 *pTemp = srcframe->imgbuf;
                    ptr   = pData->B + (pInfo->CropX ) + (pInfo->CropY ) * pData->Pitch;

                    for (i = 0; i < w; i++)
                    {
                       memcpy(pTemp + i*w, ptr + i*pData->Pitch, w);
                    }


                    ptr	= pData->G + (pInfo->CropX ) + (pInfo->CropY ) * pData->Pitch;
                    pTemp = srcframe->imgbuf + w*h;
                    for(i = 0; i < h; i++)
                    {
                       memcpy(pTemp  + i*w, ptr + i*pData->Pitch, w);
                    }

                    ptr	= pData->R + (pInfo->CropX ) + (pInfo->CropY ) * pData->Pitch;
                    pTemp = srcframe->imgbuf + 2*w*h;
                    for(i = 0; i < h; i++)
                    {
                        memcpy(pTemp  + i*w, ptr + i*pData->Pitch, w);
                    }

                  #else

                    mfxU8* ptr;

                    if (pInfo->CropH > 0 && pInfo->CropW > 0)
                    {
                        w = pInfo->CropW;
                        h = pInfo->CropH;
                    }
                    else
                    {
                        w = pInfo->Width;
                        h = pInfo->Height;
                    }

                    ptr = MSDK_MIN( MSDK_MIN(pData->R, pData->G), pData->B);
                    ptr = ptr + pInfo->CropX + pInfo->CropY * pData->Pitch;
                    mfxU8 *pTemp = srcframe->imgbuf;
                    mfxU8  *ptrB   = pTemp;
                    mfxU8  *ptrG   = pTemp + w*h;
                    mfxU8  *ptrR   = pTemp + 2*w*h;
                    for(int i = 0; i < h; i++)
                    for (int j = 0; j < w; j++)
                    {
                        ptrB[i*w + j] =  ptr[i*pData->Pitch + j*4 +0];
                        ptrG[i*w + j] =  ptr[i*pData->Pitch + j*4 +1];
                        ptrR[i*w + j] =  ptr[i*pData->Pitch + j*4 +2];
                    }
                 #endif
                    // basic info
                    srcframe->imgpts     = srcframe->imgpts;
                    srcframe->timestamp   = pipeStartTs;
                    //srcframe->pixelformat = MFX_FOURCC_RGBP; //yuv420p;
                    srcframe->realwidth   = w;
                    srcframe->realheight  = h;
                    srcframe->realstride  = w;
                    srcframe->realsize = w * h * 3;

                    pDecConfig->pmfxAllocator->Unlock(pDecConfig->pmfxAllocator->pthis, 
                                                          pSurface->Data.MemId, 
                                                          &(pSurface->Data));

				
                    // cv::Mat frame(h, w, CV_8UC4);  
                    // frame.data = temp_img_buffer;  
                    //cv::Mat frame = createMat(temp_img_buffer, w, h);
                    // cv::cvtColor(frame, srcframe->cvImg, CV_BGRA2BGR);
                    //frame.copyTo(srcframe->cvImg);

                    pDecConfig->dpipe->Store(srcframe);
                    sem_post(&gNewtaskAvaiable);

                }//if(sts==)
                
            }//for(;;) 
            nFrame++;
        }    
    }

    pDecConfig->tmEnd = std::chrono::high_resolution_clock::now();

    if(temp_img_buffer)
    {
        free(temp_img_buffer);
        temp_img_buffer = NULL;
    }
    std::cout << std::endl<< "channel("<<pDecConfig->nChannel<<") pDecoding is done\r\n"<< std::endl;

    return (void *)0;

}


#ifdef TEST_KCF_TRACK_WITH_GPU
#define MAX_NUM_TRACK_OBJECT 20
// ================= Decoding Thread =======
void *TrackerThreadFunc(void *arg)
{
    struct TrackerThreadConfig *pTrackerConfig = NULL;
    int nFrame = 0;
    mfxStatus sts = MFX_ERR_NONE;
    int  rawWidth;
    int  rawHeight; 
    std::chrono::high_resolution_clock::time_point tmStart;
    std::chrono::high_resolution_clock::time_point tmEnd;
    chrono::duration<double> diffTime;

    pTrackerConfig= (TrackerThreadConfig *) arg;
    if( NULL == pTrackerConfig)
    {
        std::cout << std::endl << "Failed Tracker Thread Configuration" << std::endl;
        return NULL;
    }
    std::cout << std::endl <<">channel("<<pTrackerConfig->nChannel<<") Initialized " << std::endl;

    bool skiptrack = false;
    String tracker_algorithm = "HOG";
    bool HOG         = true;
    bool FIXEDWINDOW = false;
    bool MULTISCALE  =  false;
    bool SILENT      = true;
    bool LAB         = false;

    rawWidth  = pTrackerConfig->width;
    rawHeight = pTrackerConfig->height;

    unsigned char* dest = (unsigned char *)malloc(rawWidth*rawHeight*3/2);
    memset(dest, 0, rawWidth*rawHeight*3/2);

     // Create KCFTracker object
     // Limit maximum 20 objects in a single frames
     KCFTracker ptracker[20];
     Detector::resultbox objectResult[20];

    for(int nloop=0; nloop<MAX_NUM_TRACK_OBJECT; nloop++)
    {
        ptracker[nloop] = new KCFTracker(HOG, FIXEDWINDOW, MULTISCALE, LAB);
    }
    int nCurTrackObjects = MAX_NUM_TRACK_OBJECT;
    int classid[MAX_NUM_TRACK_OBJECT] = {0};
    float confidence[MAX_NUM_TRACK_OBJECT] = {0.0f};
    while (grunning)
    {
        // wake-up when a new task avaiable
        std::cout << std::endl <<"waiting for a new tracker workload ready...." << std::endl;
        sem_wait(&gNewTrackTaskAvaiable[pTrackerConfig->nChannel]);
        std::cout << std::endl <<"Found a new tracker workload avaiable" << std::endl;

        for(;;)
        {
            vsource_frame_t *srcframe  = NULL;
            vector<Detector::DetctorResult> objects;

            srcframe = (vsource_frame_t*)pTrackerConfig->dpipe->Load(NULL);
            if (srcframe == NULL)
            {
                std::cout << std::endl <<" no more frames on the track queeu" << std::endl;
                break;
            }
            mfxFrameSurface1* pSurface = srcframe->pmfxSurface;
            mfxHDL handle;
            pTrackerConfig->pmfxAllocator->GetHDL(pTrackerConfig->pmfxAllocator->pthis, 
                                                      pSurface->Data.MemId, 
                                                      &(handle));

            //std::cout << std::endl <<"VASurfaceID ****: "<< *((unsigned int *)handle) << std::endl;
            Rect2d result[MAX_NUM_TRACK_OBJECT];
            if(srcframe->bROIRrefresh == true)
            {
                // ROI refresh is required, needs to wait for the new inference result
                // sem_wait();
                std::cout << std::endl <<"It is a key frame, need to wait for detection result " << std::endl;
                sem_wait(&gDetectResultAvaiable[pTrackerConfig->nChannel]);
                Detector::DetctorResult object;
                if(!gDetectResultque[pTrackerConfig->nChannel].empty()){
                    object = gDetectResultque[pTrackerConfig->nChannel].front();
                    gDetectResultque[pTrackerConfig->nChannel].pop();
                    nCurTrackObjects = (object.boxs.size() >  MAX_NUM_TRACK_OBJECT? MAX_NUM_TRACK_OBJECT:object.boxs.size());
                    std::cout << std::endl <<"Detection  result is ready: object.boxs.size()= " <<object.boxs.size()<< std::endl;
                    for(int i=0; i< nCurTrackObjects; i++)
                    {
             
                        if(CLASSES[(int)(object.boxs[i].classid)][0]=='!')
                            continue;
                        //std::cout<<"bounding box: x="<< object.boxs[0].left<< " y="<< object.boxs[0].right << " top="<<object.boxs[0].top<< "boottm="<<object.boxs[0].bottom <<std::endl;
                    }
                    tmStart = std::chrono::high_resolution_clock::now();
                    if( nCurTrackObjects == 0 /*&& object.boxs.size()>20*/){
                       skiptrack = true;
                       //std::cout << std::endl <<"no key object found, try to skip Key frame:: "<< pSurface << std::endl;
                    }else
                    {
                        for(int nloop=0; nloop< nCurTrackObjects; nloop++)
                        {
                            Rect2d boundingBox;
                            float Hfactor = rawWidth/304.0f;
                            float Vfactor = rawHeight/304.0f ;
                            boundingBox.x = object.boxs[nloop].left*Hfactor;
                            boundingBox.y = object.boxs[nloop].top*Vfactor;
                            boundingBox.width  = (object.boxs[nloop].right  - object.boxs[nloop].left)*Hfactor;
                            boundingBox.height = (object.boxs[nloop].bottom - object.boxs[nloop].top)*Vfactor;
                            confidence[nloop] = object.boxs[nloop].confidence;
                            classid[nloop] = object.boxs[nloop].classid;
                          
                            ptracker[nloop].init(boundingBox, *((unsigned int *)handle), rawWidth, rawHeight);

                            result[nloop] = boundingBox;

                            objectResult[nloop].classid    = classid[nloop];
                            objectResult[nloop].confidence = confidence[nloop];
                            objectResult[nloop].left       = (int)(result[nloop].x);
                            objectResult[nloop].top        = (int)(result[nloop].y);
                            objectResult[nloop].right      = (int)(result[nloop].width +result[nloop].x);
                            objectResult[nloop].bottom     = (int)(result[nloop].height+result[nloop].y);
                            if (objectResult[nloop].left < 0) objectResult[nloop].left = 0;
                            if (objectResult[nloop].top < 0) objectResult[nloop].top = 0;
                            if (objectResult[nloop].right >= rawWidth) objectResult[nloop].right = rawWidth - 1;
                            if (objectResult[nloop].bottom >= rawHeight) objectResult[nloop].bottom = rawHeight - 1;
                        }
                        skiptrack = false;
                        //std::cout << std::endl <<"MIN: object.boxs.size= "<<object.boxs.size()  << std::endl;
                    }
                       
                      tmEnd = std::chrono::high_resolution_clock::now();
                      diffTime  = tmEnd   - tmStart;
                      std::cout<< "Generate ROI via init takes: :" << diffTime.count()*1000 <<"(ms)"<<std::endl;
                 }
                 
             }
             else
             {
                 if(skiptrack){
                    // std::cout << std::endl <<"no need to track current frame : "<< pSurface << std::endl;
                 }else
                 {
                    //KCF_update
                    // Rect result;
                    tmStart = std::chrono::high_resolution_clock::now();
                    for(int nloop=0; nloop< nCurTrackObjects; nloop++)
                    {
                        result[nloop] = ptracker[nloop].update(*((unsigned int *)handle), rawWidth, rawHeight);
                        objectResult[nloop].classid        = classid[nloop];
                        objectResult[nloop].confidence     = confidence[nloop];
                        objectResult[nloop].left           = (int)(result[nloop].x);
                        objectResult[nloop].top            = (int)(result[nloop].y);
                        objectResult[nloop].right          = (int)(result[nloop].width +result[nloop].x);
                        objectResult[nloop].bottom         = (int)(result[nloop].height+result[nloop].y);
                        if (objectResult[nloop].left < 0) objectResult[nloop].left = 0;
                        if (objectResult[nloop].top < 0) objectResult[nloop].top = 0;
                        if (objectResult[nloop].right >= rawWidth) objectResult[nloop].right = rawWidth - 1;
                        if (objectResult[nloop].bottom >= rawHeight) objectResult[nloop].bottom = rawHeight - 1;
                    }
                    tmEnd = std::chrono::high_resolution_clock::now();
                    diffTime  = tmEnd   - tmStart;
                    std::cout<< "Generate ROI via update takes: :" << diffTime.count()*1000 <<"(ms)"<<std::endl;
                   // std::cout<<" KCFROI region.x="<< result.x << " region.y=" << result.y << " regon.w="<< result.width <<" region.h"<<result.height<<std::endl;
            }
        }

        if (FLAGS_pl){
            struct timeval timestamp;
            gettimeofday(&timestamp, NULL);
            std::cout<< "Pipeline latency " << (timestamp.tv_sec - srcframe->timestamp.tv_sec) * 1000000 + timestamp.tv_usec - srcframe->timestamp.tv_usec << "(us)" << std::endl;
        }

        // Get the Decode output surface, it is not Optimized way.
        pTrackerConfig->pmfxAllocator->Lock(pTrackerConfig->pmfxAllocator->pthis, 
                                              pSurface->Data.MemId, 
                                              &(pSurface->Data));

#if 1
        WriteRawFrameToMemory(pSurface, rawWidth, rawHeight, dest,MFX_FOURCC_NV12);
        pSurface->Data.Locked -=1;
        pTrackerConfig->pmfxAllocator->Unlock(pTrackerConfig->pmfxAllocator->pthis, 
                                              pSurface->Data.MemId, 
                                              &(pSurface->Data));

        //Got the NV12, now trying to conver to RGBP
        cv::Mat yuvimg;
        cv::Mat rgbimg;
        rgbimg.create(rawHeight,rawWidth,CV_8UC3);
        yuvimg.create(rawHeight*3/2, rawWidth, CV_8UC1);
        yuvimg.data = dest;
        cv::cvtColor(yuvimg, rgbimg, CV_YUV2BGR_I420);

        //cv::namedWindow( "Display window", cv::WINDOW_AUTOSIZE );
        //cv::imshow( "Display window", rgbimg );  
        //cv::waitKey(100);

        each_frame[0] +=1;
        total_frame[0]++;
        vector<Detector::DetctorResult> objects2;
        objects2.resize(1);

        objects2[0].imgsize.width  = rawWidth;
        objects2[0].imgsize.height = rawHeight;
        objects2[0].inputid = 0;
        objects2[0].orgimg  = rgbimg;
        objects2[0].frameno = 1;
        objects2[0].channelid = 0 ;


        for(int nloop=0; nloop< nCurTrackObjects && skiptrack == false; nloop++){
        //if( objectResult[nloop].confidence > 0.3)
        objects2[0].boxs.push_back(objectResult[nloop]);
        }
        pthread_mutex_lock(&mutexshow); 	
        gresultque.push(objects2);
        pthread_mutex_unlock(&mutexshow); 	
        sem_post(&g_semtshow);
#endif
        pTrackerConfig->dpipe->Put(srcframe);


       }// for (auto& dpipe : *(pScheConfig->pvdpipe)) 
   }//while   
   return (void *)0;

}
#endif
class ScheduleThreadConfig
{
 public:
    ScheduleThreadConfig():
        gNet_input_width(0),
        gNet_input_height(0),

        pvideo(NULL),
        nChannel(0),
        totalInferNum(0),
        bStartCount(false),
        bTerminated(false)
    {
    };
    int gNet_input_width;
    int gNet_input_height;

    VideoWriter       *pvideo;
    int                nChannel;
    vector<VaDualPipe *> *pvdpipe;
    int                totalInferNum;
    bool               bStartCount;
    bool               bTerminated;
 };

void *ScheduleThreadFunc(void *arg)
{
    std::cout <<" Schedue Func thread called" << std::endl;
        
    struct ScheduleThreadConfig *pScheConfig = NULL;
    bool bret    = false;
    int  nFrame = 0;
    int nInferThreadInstance = 0;
    Detector::InsertImgStatus faceret=Detector::INSERTIMG_NULL;
    vector<Detector::DetctorResult> objects;
    int fpsCount = 0;
    std::chrono::high_resolution_clock::time_point staticsStart, staticsEnd;
    pScheConfig= (ScheduleThreadConfig *) arg;
    if( NULL == pScheConfig)
    {
        std::cout << std::endl << "Failed Inference Thread Configuration" << std::endl;
        return NULL;
    }
    pScheConfig->totalInferNum =0;
    
#ifndef ENABLE_WORKLOAD_BALANCE
 while (grunning)
   {
        // wake-up when a new task avaiable
        sem_wait(&gNewtaskAvaiable);
        for (auto& dualpipe : *(pScheConfig->pvdpipe)) 
        {
             vsource_frame_t *srcframe  = NULL;
             srcframe = (vsource_frame_t*)dualpipe->Load(NULL);
             if( srcframe == NULL ) {
                 continue;
             }
                
             pScheConfig->totalInferNum++;
             fpsCount++;
             if(fpsCount == 1){
                 staticsStart = std::chrono::high_resolution_clock::now();
             }else if(fpsCount == 100){
                 staticsEnd = std::chrono::high_resolution_clock::now();

                 chrono::duration<double> diffTime  = staticsEnd   - staticsStart;
                 double fps = (100*1000/(diffTime.count()*1000.0));
                 total_fps = fps;
                 std::cout<< "Schedule FPS:" << fps <<"(f/s)"<<std::endl;
                 fpsCount = 0;
             }
       
             //cv::Mat frame = srcframe->cvImg;
             cv::Mat frame = createMat(srcframe->imgbuf, gNet_input_width, gNet_input_height);
             dualpipe->Put(srcframe);


             std::chrono::high_resolution_clock::time_point staticsStart3, staticsEnd3;
             staticsStart3 = std::chrono::high_resolution_clock::now();

             if(FLAGS_pv){
                 // Enable performance dump
                   struct timeval timestamp;
                   gettimeofday(&timestamp, NULL);
                   std::cout<< "submit inference frame " << timestamp.tv_sec * 1000000 + timestamp.tv_usec << "(us) from channelID="<<srcframe->channel<< " frameNo=" << srcframe->frameno<<std::endl;
             }
             faceret = gDetector[0].InsertImage(frame, objects, srcframe->channel, srcframe->frameno);
             staticsEnd3 = std::chrono::high_resolution_clock::now();
             std::chrono::duration<double> diffTime3  = staticsEnd3   - staticsStart3;
             //std::cout <<" Infer:" << diffTime3.count()*1000.0<<"ms"<<std::endl;	
             if (Detector::INSERTIMG_GET == faceret ||Detector::INSERTIMG_PROCESSED == faceret)     {   //aSync call, you must use the ret image
#ifdef TEST_KCF_TRACK_WITH_GPU
                std::cout <<" Infer:" << " done one frame : objects= "<< objects.size() << std::endl; 

                for(int k=0;k<objects.size();k++){
                    // inform the queue of the channelid
                    // pthread_mutex_lock(&mutexshow);	
                    std::cout <<" Infer:" << " push detect result to Detect Result queue: "<< objects[k].channelid << " boxes=" <<objects[k].boxs.size()  <<std::endl; 
                    gDetectResultque[objects[k].channelid].push(objects[k]);
                    //pthread_mutex_unlock(&mutexshow);	
                    sem_post(&gDetectResultAvaiable[objects[k].channelid]);
                }  

#else
                 for(int k=0;k<objects.size();k++){
                     each_frame[objects[k].inputid]+=1;
                     total_frame[0]++;

                     if(FLAGS_pv){
                         // Enable performance dump
                         struct timeval timestamp;
                         gettimeofday(&timestamp, NULL);
                         std::cout<< "Finish inference frame " << timestamp.tv_sec * 1000000 + timestamp.tv_usec << "(us) from channelID="<<objects[k].inputid<< " frameNo=" << objects[k].frameno<<std::endl;
                    }
                 }
                 if (FLAGS_pl){
                     struct timeval timestamp;
                     gettimeofday(&timestamp, NULL);
                     std::cout<< "Pipeline latency " << (timestamp.tv_sec - srcframe->timestamp.tv_sec) * 1000000 + timestamp.tv_usec - srcframe->timestamp.tv_usec << "(us)" << std::endl;
                 }
                 if( Detector::INSERTIMG_GET == faceret && FLAGS_show){
                     pthread_mutex_lock(&mutexshow); 	
                     gresultque.push(objects);
                     pthread_mutex_unlock(&mutexshow); 
                     sem_post(&g_semtshow);

                     pthread_mutex_lock(&mutexshow); 
                     while(gresultque.size()>=2 && grunning){  //only cache 2 batch
                         pthread_mutex_unlock(&mutexshow); 	
                         usleep(1*1000); //sleep 2ms to recheck
                         pthread_mutex_lock(&mutexshow); 
                     }
                     pthread_mutex_unlock(&mutexshow); 
                 }
                
#endif 
            }//if (Detector::INSERTIMG_GET 
       }// for (auto& dpipe : *(pScheConfig->pvdpipe)) 
   }//while
#else
    while (grunning)
    {
        // wake-up when a new task avaiable
        sem_wait(&gNewtaskAvaiable);
        // scan all the input channel to get a new task
        // Make sure all channel has the same priority
        for (auto& dualpipe : *(pScheConfig->pvdpipe)) 
        {
            vsource_frame_t *srcframe  = NULL;
            srcframe = (vsource_frame_t*)dualpipe->LoadNoWait();
            if( srcframe == NULL ) 
            {
                continue;
            }
            infer_task_t task;
            task.dpipe   = dualpipe;
            task.dbuffer = srcframe;

            pScheConfig->totalInferNum++;
#if 1
          
            int      nChannel  = 0;

            nChannel = srcframe->channel;
            if(nChannel == 0)
            {
               nInferThreadInstance = 1;
               pthread_mutex_lock(&mutexinfer[nInferThreadInstance]);     
               gtaskque[nInferThreadInstance].push(task);
               pthread_mutex_unlock(&mutexinfer[nInferThreadInstance]); 
               sem_post(&gsemtInfer[nInferThreadInstance]);
               nFrame++;
            }else
            {
           
               nInferThreadInstance = 0;
               pthread_mutex_lock(&mutexinfer[nInferThreadInstance]);     
               gtaskque[nInferThreadInstance].push(task);
               pthread_mutex_unlock(&mutexinfer[nInferThreadInstance]); 
               sem_post(&gsemtInfer[nInferThreadInstance]);
               nFrame++; 
           }
#else
            if(nInferThreadInstance == NUM_OF_GPU_INFER -1 )
            {
                // In case there are two CPU nodes if the CPU is power enough
                static unsigned int toggleCPU =0;
                toggleCPU++;
                if( (NUM_OF_GPU_INFER -1 ) == 1) toggleCPU =0;


                if(nFrame < (NUM_OF_GPU_INFER -1)*2)
                {
                    pthread_mutex_lock(&mutexinfer[nInferThreadInstance -toggleCPU%2]);     
                    gtaskque[nInferThreadInstance-toggleCPU%2].push(task);
                    pthread_mutex_unlock(&mutexinfer[nInferThreadInstance-toggleCPU%2]); 
                    sem_post(&gsemtInfer[nInferThreadInstance-toggleCPU%2]);
                    nFrame++;
                 }
                 // Change the next infernece deve as GPU
                 if(nFrame == (NUM_OF_GPU_INFER -1)*2)
                 {
                     nInferThreadInstance = 0;
                     nFrame = 0;
                 }
             }else{
                 // Push batch size buffer to GPU 
                 if(nFrame < g_batchSize)
                 {
                     pthread_mutex_lock(&mutexinfer[nInferThreadInstance]);     
                     gtaskque[nInferThreadInstance].push(task);
                     pthread_mutex_unlock(&mutexinfer[nInferThreadInstance]); 
                     sem_post(&gsemtInfer[nInferThreadInstance]);
                     nFrame++;
                 }
                 // change he next Node as CPU
                 if(nFrame == g_batchSize)
                 {
                      nInferThreadInstance = nInferThreadInstance==0? (NUM_OF_GPU_INFER -1):0 ;
                      nFrame = 0;
                 }
             }//if(nInferThreadInstance == NUM_OF_GPU_INFER -1 )
 #endif
      }// for (auto& dpipe : *(pScheConfig->pvdpipe))
     
   }//while
  
#endif
	
exit:
	
    std::cout << std::endl<<  "Schedule thread is done\r\n"<< std::endl;
 
    return NULL;


}


class InferThreadConfig
 {
 public:
    InferThreadConfig():
        pvideo(NULL),
        nChannel(0),
        totalInferNum(0),
        bStartCount(false),
        bTerminated(false),
        nBatch(0)
    {
    };

    VideoWriter       *pvideo;
    int                nChannel;
    vector<VaDualPipe *> *pvdpipe;
    int                totalInferNum;
    bool               bStartCount;
    bool               bTerminated;
    int nBatch;
 };
inline int set_cpu(int i)
{
    cpu_set_t mask;
    CPU_ZERO(&mask);
 
    CPU_SET(i,&mask);

    std::cout<<"thread "<<pthread_self()<<" i="<<i<<std::endl;
    if(-1 == pthread_setaffinity_np(pthread_self() ,sizeof(mask),&mask))
    {
        return -1;
    }
    return 0;
}
void *InferThreadFunc(void *arg)
{
    struct InferThreadConfig *pInferConfig = NULL;
    int  nFrame = 0;
    pInferConfig= (InferThreadConfig *) arg;
    if( NULL == pInferConfig)
    {
        std::cout << std::endl << "Failed Inference Thread Configuration" << std::endl;
        return NULL;
    }
    pInferConfig->totalInferNum =0;

    std::chrono::high_resolution_clock::time_point staticsStart, staticsEnd;
    staticsStart = std::chrono::high_resolution_clock::now();


    Detector::InsertImgStatus faceret=Detector::INSERTIMG_NULL;
    int fpsCount = 0;
    set_cpu(pInferConfig->nChannel);
    
    std::cout <<" InferThreadFunc called : channelID " << pInferConfig->nChannel << std::endl;
    while (grunning)
    {
        sem_wait(&gsemtInfer[pInferConfig->nChannel]);
        while(!gtaskque[pInferConfig->nChannel].empty()){
            
            pthread_mutex_lock(&mutexinfer[pInferConfig->nChannel]);     
            infer_task_t task = gtaskque[pInferConfig->nChannel].front();   
            gtaskque[pInferConfig->nChannel].pop();           
            pthread_mutex_unlock(&mutexinfer[pInferConfig->nChannel]); 

            vsource_frame_t *srcframe  = NULL;
            srcframe = (vsource_frame_t*)task.dbuffer;
            
            int              nChannel  = 0;

            nFrame++;
            pInferConfig->totalInferNum++;
			
            nChannel = srcframe->channel;

            fpsCount++;
            if(fpsCount == 1){
                staticsStart = std::chrono::high_resolution_clock::now();
            }else if(fpsCount == 100){
                staticsEnd = std::chrono::high_resolution_clock::now();
	
                chrono::duration<double> diffTime  = staticsEnd   - staticsStart;
                double fps = (100*1000/(diffTime.count()*1000.0));
                std::cout<< "Infer  "<< pInferConfig->nChannel<< " FPS:" << fps <<"(f/s)"<<std::endl;
                fpsCount = 0;
            }
            // Maybe this can be moved to scheduler.
            //cv::Mat frame = srcframe->cvImg; 
            cv::Mat frame = createMat(srcframe->imgbuf, gNet_input_width, gNet_input_height);
            task.dpipe->Put(srcframe);
            vector<Detector::DetctorResult> objects;
            faceret = gDetector[pInferConfig->nChannel].InsertImage(frame,objects,srcframe->channel, srcframe->frameno);	
	
            if (Detector::INSERTIMG_GET == faceret ||Detector::INSERTIMG_PROCESSED == faceret)     {  //aSync call, you must use the ret image
                for(int k=0;k<objects.size();k++){
                    each_frame[objects[k].inputid]+=1;
                    total_frame[pInferConfig->nChannel]++;	
                }	
                if( Detector::INSERTIMG_GET == faceret){
                   if(pInferConfig->nChannel ==0 ){
                       if(FLAGS_show){
		            pthread_mutex_lock(&mutexshow); 	
		            gresultque.push(objects);
		            pthread_mutex_unlock(&mutexshow); 	
		            sem_post(&g_semtshow);
					
		            pthread_mutex_lock(&mutexshow); 
		            while(gresultque.size()>=2 && grunning){  //only cache 2 batch
		                pthread_mutex_unlock(&mutexshow); 	
		                usleep(1*1000); //sleep 2ms to recheck
		                pthread_mutex_lock(&mutexshow); 
		            }			
		            pthread_mutex_unlock(&mutexshow);
                        } 
                    }else{
                        if(FLAGS_show){
		            pthread_mutex_lock(&mutexshow); 	
		            gresultque1.push(objects);
		            pthread_mutex_unlock(&mutexshow); 	
		            sem_post(&g_semtshow);
                        }
			/*		
		            pthread_mutex_lock(&mutexshow); 
		            while(gresultque1.size()>=2 && grunning){  //only cache 2 batch
		                pthread_mutex_unlock(&mutexshow); 	
		                usleep(1*1000); //sleep 2ms to recheck
		                pthread_mutex_lock(&mutexshow); 
		            }			
		            pthread_mutex_unlock(&mutexshow);*/
                    }
          		
               }//if( Detector::INSERTIMG_GET == faceret){			
            }// if (Detector::INSERTIMG_GET == faceret ||D

        } // while(!gtaskque[pInferConfig->nChannel].empty()){     
    }//while (grunning)
   

    std::cout<< "Inference thread "<<pInferConfig->nChannel <<" is done\r\n"<< std::endl;
 
    return (void *)0;
}

int
alignment(void *ptr, int alignto) {
	int mask = alignto - 1;
#ifdef __x86_64__
	return alignto - (((unsigned long long) ptr) & mask);
#else
	return alignto - (((unsigned) ptr) & mask);
#endif
}
#define VSOURCE_ALIGNMENT 16

vsource_frame_t *
vsource_frame_init(int channel, vsource_frame_t *frame) {
    int i =0;


    bzero(frame, sizeof(vsource_frame_t));
    //
  
    frame->linesize[i] = gNet_input_width;
    
    frame->maxstride =0;
    frame->imgbufsize = gNet_input_width * gNet_input_height *3;
    frame->imgbuf = ((unsigned char *) frame) + sizeof(vsource_frame_t);
    frame->imgbuf += alignment(frame->imgbuf, VSOURCE_ALIGNMENT);
  
    bzero(frame->imgbuf, frame->imgbufsize);
    return frame;
}

void dualpipe_buffer_init(void *buffer)
{
    vsource_frame_init(0, (vsource_frame_t *)buffer);
}

int main(int argc, char *argv[])
{
    bool bret = false;
    vector<VaDualPipe *>  vdpipe;
    float normalize_factor = 1.0;
    VaDualPipe *dpipe[NUM_OF_CHANNELS];
    VaDualPipe *dKCFpipe[NUM_OF_CHANNELS];

    std::vector<pthread_t>    vScheduleThreads;
    std::vector<pthread_t>    vInferThreads;
    std::vector<pthread_t>    vDecThreads;
    std::vector<pthread_t>    vKCFTrackerThreads;	
    std::vector<pthread_t>    vAssistThreads;
    std::vector<InferThreadConfig *> vpInferThreadConfig;
    std::vector<ScheduleThreadConfig *> vpScheduleThreadConfig;
    // =================================================================

    // Parse command line parameters
    gflags::ParseCommandLineNonHelpFlags(&argc, &argv, true);

    if (FLAGS_h) {
        App_ShowUsage();
        return 0;
    }

    if (FLAGS_l.empty()) {
        std::cout << " [error] Labels file path not set" << std::endl;
        App_ShowUsage();
        return 1;
    }

    bool noPluginAndBadDevice = FLAGS_p.empty() && FLAGS_d.compare("CPU")
                                && FLAGS_d.compare("GPU");
    if (FLAGS_i.empty() || FLAGS_m.empty() || noPluginAndBadDevice) {
        if (noPluginAndBadDevice)
            std::cout << " [error] Device is not supported" << std::endl;
        if (FLAGS_m.empty())
            std::cout << " [error] File with model - not set" << std::endl;
        if (FLAGS_i.empty())
            std::cout << " [error] Image(s) for inference - not set" << std::endl;
        App_ShowUsage();
        return 1;
    }

    // prepare video input
    std::cout << std::endl;

    std::string input_filename[NUM_OF_CHANNELS] = { FLAGS_i};
    for(int nloop=0; nloop< NUM_OF_CHANNELS; nloop++)
    {
        input_filename[nloop] = FLAGS_i;
    }

// =================================================================
// Intel Media SDK
//
// if input file has ".264" extension, let assume we want to enable
// media sdk to decode video elementary stream here.
    bool bUseMediaSDK = true;

    std::cout << "> Use [Intel Media SDK] and [Intel OpenVINO SDK]." << std::endl;


#ifdef WIN32
    std::string archPath = "../../../bin" OS_LIB_FOLDER "intel64/Release/";
#else
    std::string archPath = "../../../lib/" OS_LIB_FOLDER "intel64";
#endif


//-------------------------------------------
// Inference Engine Initialization
    std::cout << std::endl << "> Init OpenVINO Inference session." << std::endl;
    VideoWriter *video[NUM_OF_CHANNELS];

    for(int nLoop=0; nLoop< NUM_OF_GPU_INFER ; nLoop++)
    {
        int ret;
        cv::Size net_size;
        char szBuffer[256]={0};
        std::string binFileName = fileNameNoExt(FLAGS_m) + ".bin";
        if(nLoop==0){
            std::string device = FLAGS_d;
            ret = gDetector[nLoop].Load(device, FLAGS_m, binFileName, FLAGS_batch);
            if(ret < 0){
                std::cout << "Failed to initialize object detector model" << std::endl;
                return 1;
            }
#ifdef TEST_KCF_TRACK_WITH_GPU
            gDetector[nLoop].SetMode(false);
#endif
        }else{
            std::string device = "CPU";
            //gDetector[nLoop].Load(device, FLAGS_m, binFileName, FLAGS_batch);
            ret = gDetector[nLoop].Load(device, "../../test_content/IR/SSD_mobilenet/MobileNetSSD_deploy_32.xml", "../../test_content/IR/SSD_mobilenet/MobileNetSSD_deploy_32.bin",NUM_OF_CPU_BATCH);
#ifdef TEST_KCF_TRACK_WITH_GPU            
            gDetector[nLoop].SetMode(false);
#endif
        }
        if(ret != 0){
            std::cout<< "Failed to load the provided model files, either file not found or datatype is not supported by selected device"<<std::endl;
            return 1;
        }  
        g_batchSize = FLAGS_batch;
        net_size = gDetector[nLoop].GetNetSize();
        gNet_input_width = net_size.width;
        gNet_input_height = net_size.height;
        std::cout << "\t gNet_input_width: " 
       <<gNet_input_width<<" gNet_input_height: "<<gNet_input_height<< std::endl;

        // Prepare output file to archive result video
        sprintf(szBuffer,"out_%d.h264",nLoop);
        
        sem_init(&gsemtInfer[nLoop], 0, 0);
        pthread_mutex_init(&mutexinfer[nLoop],NULL);
    }

    std::cout << ">  Initialize OpenVNIO session success." << std::endl;
    std::cout << ">  start to initialize decoding sessions with MSDK." << std::endl;


// Done, Inference Engine Initialization

    if(FLAGS_show == true){
        if(FLAGS_c > 5 ){
           std::cout << "\t. current version only support display maximum 6  streams." << std::endl;
           return 1;
        }
        // Initialize display thread if needed
        DisplayThreadConfig *pDispThreadConfig = new DisplayThreadConfig();
        memset(pDispThreadConfig, 0, sizeof(DisplayThreadConfig));
#ifdef TEST_KCF_TRACK_WITH_GPU
        pDispThreadConfig->nCellWidth            = 640;//gNet_input_width;
        pDispThreadConfig->nCellHeight           = 480;//gNet_input_height;
        pDispThreadConfig->nRows                 = 1;//2;
        pDispThreadConfig->nCols                 = 1;//3;
#else
        pDispThreadConfig->nCellWidth            = gNet_input_width;
        pDispThreadConfig->nCellHeight           = gNet_input_height;
        pDispThreadConfig->nRows                 = 2;
        pDispThreadConfig->nCols                 = 3;
#endif
        sem_init(&g_semtshow, 0, 0);
        pthread_mutex_init(&mutexshow,NULL);
        pthread_t threadid;
        pthread_create(&threadid, NULL, DisplayThreadFunc, (void *)(pDispThreadConfig));

        vAssistThreads.push_back(threadid) ;     
    }
    if(1){
        pthread_t perfThreadid;
        pthread_create(&perfThreadid, NULL, thr_fps, NULL);
        vAssistThreads.push_back(perfThreadid) ;     
    }

    //Initialize dpipe between decoding and scheduler thread
    for(int nLoop=0; nLoop< FLAGS_c; nLoop++)
    {
        // Initialize dbuffer between decoding thread and inference thread.
        // 4 buffer for each dpipe buffer
        char szBuffer[256]={0};
        sprintf(szBuffer, "SharedInferBuf%d", nLoop);
        dpipe[nLoop] = new VaDualPipe();
        if( dpipe[nLoop] == NULL ) {
            std::cout<<"create dst-pipeline failed .\n"<<std::endl;
            return 1;
        }
        dpipe[nLoop]->Initialize(100, sizeof(vsource_frame_t) + gNet_input_width*gNet_input_height*4, dualpipe_buffer_init);
	
        vdpipe.push_back(dpipe[nLoop]);

#ifdef TEST_KCF_TRACK_WITH_GPU	
		// Initialize the buffer for KCF tracker
        // 20 buffer for each dpipe buffer
        memset(szBuffer, 0, 256);
        sprintf(szBuffer, "SharedKCFBuf%d", nLoop);
        dKCFpipe[nLoop] = new VaDualPipe();
        if( dKCFpipe[nLoop] == NULL ) {
            std::cout<<"create dst-pipeline failed .\n"<<std::endl;
            return 1;
        }
        dKCFpipe[nLoop]->Initialize(20, sizeof(vsource_frame_t) + gNet_input_width*gNet_input_height*4, dualpipe_buffer_init);
      
		//TODO: need to track it?
        //vdpipe.push_back(dpipe[nLoop]);
#endif
    }
   

//-------------------------------------------
    Mat frame;

    bool no_more_data = false;
    int totalFrames = 0;

    // =================================================================
    // Intel Media SDK
    //
    // Use Intel Media SDK for decoding and VPP (resizing)

    mfxStatus sts;
    mfxIMPL impl; 				  // SDK implementation type: hardware accelerator?, software? or else
    mfxVersion ver;				  // media sdk version

    // mfxSession and Allocator can be shared globally.
    MFXVideoSession   mfxSession[NUM_OF_CHANNELS];
    mfxFrameAllocator mfxAllocator[NUM_OF_CHANNELS];	 

    FILE *f_i[NUM_OF_CHANNELS];
    mfxBitstream mfxBS[NUM_OF_CHANNELS];
    mfxFrameSurface1** pmfxDecSurfaces[NUM_OF_CHANNELS];
    mfxFrameSurface1** pmfxVPP_In_Surfaces[NUM_OF_CHANNELS];
    mfxFrameSurface1** pmfxVPP_Out_Surfaces[NUM_OF_CHANNELS];

    std::vector<MFXVideoDECODE *>    vpMFXDec;
    std::vector<MFXVideoVPP *>       vpMFXVpp;

    std::cout << "channels of stream:"<<FLAGS_c << std::endl;
    std::cout << " FPS of each inference workload:"<<FLAGS_fps << std::endl; 

    for(int nLoop=0; nLoop< FLAGS_c; nLoop++)
    {
        // =================================================================
         // Intel Media SDK
        std::cout << "\t. Open input file: " << input_filename[nLoop] << std::endl;
       
        f_i[nLoop] = fopen(input_filename[nLoop].c_str(), "rb");
        if(f_i[nLoop] ==0 ){
           std::cout << "\t.Failed to  open input file: " << input_filename[nLoop] << std::endl;
           goto exit_here;
        }
        //f_o = fopen("./resized.rgb32", "wb");

        // Initialize Intel Media SDK session
        // - MFX_IMPL_AUTO_ANY selects HW accelaration if available (on any adapter)
        // - Version 1.0 is selected for greatest backwards compatibility.
        //   If more recent API features are needed, change the version accordingly

        std::cout << std::endl << "> Declare Intel Media SDK video session and Init." << std::endl;
        sts = MFX_ERR_NONE;
        impl = MFX_IMPL_AUTO_ANY;
        ver = { {0, 1} };
        sts = Initialize(impl, ver, &mfxSession[nLoop], &mfxAllocator[nLoop]);
        if( sts != MFX_ERR_NONE){
            std::cout << "\t. Failed to initialize decode session" << std::endl;
            goto exit_here;
        }

        MFXVideoDECODE *pmfxDEC = new MFXVideoDECODE(mfxSession[nLoop]);
        vpMFXDec.push_back(pmfxDEC);
        MFXVideoVPP    *pmfxVPP = new MFXVideoVPP(mfxSession[nLoop]);
        vpMFXVpp.push_back(pmfxVPP);
        if( pmfxDEC == NULL || pmfxVPP == NULL ){
            std::cout << "\t. Failed to initialize decode session" << std::endl;
            goto exit_here;
        }

        // [DECODER]
        // Initialize decode video parameters
        // - In this example we are decoding an AVC (H.264) stream
        // - For simplistic memory management, system memory surfaces are used to store the decoded frames
        std::cout << "\t. Start preparing video parameters for decoding." << std::endl;
        mfxExtDecVideoProcessing DecoderPostProcessing;
        mfxVideoParam DecParams;
        bool VD_LP_resize = false; // default value is for VDBOX+SFC case
        if (0 == FLAGS_dec_postproc.compare("off"))
            VD_LP_resize = false;
        else
            VD_LP_resize = true;
        memset(&DecParams, 0, sizeof(DecParams));
        DecParams.mfx.CodecId = MFX_CODEC_AVC;                  // h264
        DecParams.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;   // will use GPU memory in this example

        std::cout << "\t\t.. Suppport H.264(AVC) and use video memory." << std::endl;

        // Prepare media sdk bit stream buffer
        // - Arbitrary buffer size for this example        
        memset(&mfxBS[nLoop], 0, sizeof(mfxBS[nLoop]));
        mfxBS[nLoop].MaxLength = 1024 * 1024;
        mfxBS[nLoop].Data = new mfxU8[mfxBS[nLoop].MaxLength];
        MSDK_CHECK_POINTER(mfxBS[nLoop].Data, MFX_ERR_MEMORY_ALLOC);

        // Read a chunk of data from stream file into bit stream buffer
        // - Parse bit stream, searching for header and fill video parameters structure
        // - Abort if bit stream header is not found in the first bit stream buffer chunk
        sts = ReadBitStreamData(&mfxBS[nLoop], f_i[nLoop]);
        
        sts = pmfxDEC->DecodeHeader(&mfxBS[nLoop], &DecParams);
        MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
        
        std::cout << "\t. Done preparing video parameters." << std::endl;

        // [VPP]
        // Initialize VPP parameters
        // - For simplistic memory management, system memory surfaces are used to store the raw frames
        //   (Note that when using HW acceleration D3D surfaces are prefered, for better performance)
        std::cout << "\t. Start preparing VPP In/ Out parameters." << std::endl;

#ifdef TEST_KCF_TRACK_WITH_GPU	
        mfxU32 fourCC_VPP_Out =  MFX_FOURCC_RGB4;
        mfxU32 chromaformat_VPP_Out = MFX_CHROMAFORMAT_YUV444;
        mfxU8 bpp_VPP_Out     = 32;
        mfxU8 channel_VPP_Out = 4;
        mfxU16 nDecSurfNum= 0;
#else
        mfxU32 fourCC_VPP_Out =  MFX_FOURCC_RGBP;
        mfxU32 chromaformat_VPP_Out = MFX_CHROMAFORMAT_YUV444;
        mfxU8 bpp_VPP_Out     = 24;
        mfxU8 channel_VPP_Out = 3;
        mfxU16 nDecSurfNum= 0;
#endif

        mfxVideoParam VPPParams;
        mfxExtVPPScaling scalingConfig;
        memset(&VPPParams, 0, sizeof(VPPParams));
        memset(&scalingConfig, 0, sizeof(mfxExtVPPScaling));

        mfxExtBuffer *pExtBuf[1];
        mfxExtBuffer *pExtBufDec[1];
        VPPParams.ExtParam = (mfxExtBuffer **)pExtBuf;
        scalingConfig.Header.BufferId    = MFX_EXTBUFF_VPP_SCALING;
        scalingConfig.Header.BufferSz    = sizeof(mfxExtVPPScaling);
        scalingConfig.ScalingMode        = MFX_SCALING_MODE_LOWPOWER;
        VPPParams.ExtParam[0] = (mfxExtBuffer*)&(scalingConfig);
        VPPParams.NumExtParam = 1;
        printf("Num of ext params=%d\r\n",VPPParams.NumExtParam );


        // Video processing input data format / decoded frame information
        VPPParams.vpp.In.FourCC         = MFX_FOURCC_NV12;
        VPPParams.vpp.In.ChromaFormat   = MFX_CHROMAFORMAT_YUV420;
        VPPParams.vpp.In.CropX          = 0;
        VPPParams.vpp.In.CropY          = 0;
        VPPParams.vpp.In.CropW          = DecParams.mfx.FrameInfo.CropW;
        VPPParams.vpp.In.CropH          = DecParams.mfx.FrameInfo.CropH;
        VPPParams.vpp.In.PicStruct      = MFX_PICSTRUCT_PROGRESSIVE;
        VPPParams.vpp.In.FrameRateExtN  = 30;
        VPPParams.vpp.In.FrameRateExtD  = 1;
        // width must be a multiple of 16
        // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
        VPPParams.vpp.In.Width  = MSDK_ALIGN16(VPPParams.vpp.In.CropW);
        VPPParams.vpp.In.Height = (MFX_PICSTRUCT_PROGRESSIVE == VPPParams.vpp.In.PicStruct)?
                                  MSDK_ALIGN16(VPPParams.vpp.In.CropH) : MSDK_ALIGN32(VPPParams.vpp.In.CropH);

        // Video processing output data format / resized frame information for inference engine
        VPPParams.vpp.Out.FourCC        = fourCC_VPP_Out;
        VPPParams.vpp.Out.ChromaFormat  = chromaformat_VPP_Out;
        VPPParams.vpp.Out.CropX         = 0;
        VPPParams.vpp.Out.CropY         = 0;
        VPPParams.vpp.Out.CropW         = gNet_input_width; // SSD: 300, YOLO-tiny: 448
        VPPParams.vpp.Out.CropH         = gNet_input_height;    // SSD: 300, YOLO-tiny: 448
        VPPParams.vpp.Out.PicStruct     = MFX_PICSTRUCT_PROGRESSIVE;
        VPPParams.vpp.Out.FrameRateExtN = 30;
        VPPParams.vpp.Out.FrameRateExtD = 1;
        // width must be a multiple of 16
        // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
        VPPParams.vpp.Out.Width  = MSDK_ALIGN16(VPPParams.vpp.Out.CropW);
        VPPParams.vpp.Out.Height = (MFX_PICSTRUCT_PROGRESSIVE == VPPParams.vpp.Out.PicStruct)?
                                   MSDK_ALIGN16(VPPParams.vpp.Out.CropH) : MSDK_ALIGN32(VPPParams.vpp.Out.CropH);

        VPPParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;

        std::cout << "\t. VPP ( " << VPPParams.vpp.In.CropW << " x " << VPPParams.vpp.In.CropH << " )"
                  << " -> ( " << VPPParams.vpp.Out.CropW << " x " << VPPParams.vpp.Out.CropH << " )" << ", ( " << VPPParams.vpp.Out.Width << " x " << VPPParams.vpp.Out.Height << " )" << std::endl;

        std::cout << "\t. Done preparing VPP In/ Out parameters." << std::endl;

        /* LP size */
        if (VD_LP_resize)
        {
            if ( (MFX_CODEC_AVC == DecParams.mfx.CodecId) && /* Only for AVC */
                 (MFX_PICSTRUCT_PROGRESSIVE == DecParams.mfx.FrameInfo.PicStruct)) /* ...And only for progressive!*/
            {   /* it is possible to use decoder's post-processing */
                DecoderPostProcessing.Header.BufferId    = MFX_EXTBUFF_DEC_VIDEO_PROCESSING;
                DecoderPostProcessing.Header.BufferSz    = sizeof(mfxExtDecVideoProcessing);
                DecoderPostProcessing.In.CropX = 0;
                DecoderPostProcessing.In.CropY = 0;
                DecoderPostProcessing.In.CropW = DecParams.mfx.FrameInfo.CropW;
                DecoderPostProcessing.In.CropH = DecParams.mfx.FrameInfo.CropH;

                DecoderPostProcessing.Out.FourCC = DecParams.mfx.FrameInfo.FourCC;
                DecoderPostProcessing.Out.ChromaFormat = DecParams.mfx.FrameInfo.ChromaFormat;
                DecoderPostProcessing.Out.Width = MSDK_ALIGN16(VPPParams.vpp.Out.Width);
                DecoderPostProcessing.Out.Height = MSDK_ALIGN16(VPPParams.vpp.Out.Height);
                DecoderPostProcessing.Out.CropX = 0;
                DecoderPostProcessing.Out.CropY = 0;
                DecoderPostProcessing.Out.CropW = VPPParams.vpp.Out.CropW;
                DecoderPostProcessing.Out.CropH = VPPParams.vpp.Out.CropH;
                DecParams.ExtParam = (mfxExtBuffer **)pExtBufDec;
                DecParams.ExtParam[0] = (mfxExtBuffer*)&(DecoderPostProcessing);
                DecParams.NumExtParam = 1;
                std::cout << "\t.Decoder's post-processing is used for resizing\n"<< std::endl;

                /* need to correct VPP params: re-size done after decoding
                 * So, VPP for CSC NV12->RGBP only */
                VPPParams.vpp.In.Width = DecoderPostProcessing.Out.Width;
                VPPParams.vpp.In.Height = DecoderPostProcessing.Out.Height;
                VPPParams.vpp.In.CropW = VPPParams.vpp.Out.CropW;
                VPPParams.vpp.In.CropH = VPPParams.vpp.Out.CropH;
                /* scaling is off (it was configured via extended buffer)*/
                VPPParams.NumExtParam = 0;
                VPPParams.ExtParam = NULL;
            }
        }
        // [DECODER]
        // Query number of required surfaces
        std::cout << "\t. Query number of required surfaces for decoder and memory allocation." << std::endl;

        mfxFrameAllocRequest DecRequest = { 0 };
        sts = pmfxDEC->QueryIOSurf(&DecParams, &DecRequest);
        MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
        
        DecRequest.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_FROM_VPPIN;
        DecRequest.Type |= MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET;

        // Try to add two more surfaces
        DecRequest.NumFrameMin +=2;
        DecRequest.NumFrameSuggested +=2;

        // [VPP]
        // Query number of required surfaces
        std::cout << "\t. Query number of required surfaces for VPP and memory allocation." << std::endl;

        mfxFrameAllocRequest VPPRequest[2];// [0] - in, [1] - out
        memset(&VPPRequest, 0, sizeof(mfxFrameAllocRequest) * 2);
        sts = pmfxVPP->QueryIOSurf(&VPPParams, VPPRequest);
        
        // [DECODER]
        // memory allocation
        mfxFrameAllocResponse DecResponse = { 0 };
        sts = mfxAllocator[nLoop].Alloc(mfxAllocator[nLoop].pthis, &DecRequest, &DecResponse);
        if(MFX_ERR_NONE > sts)
        {
            MSDK_PRINT_RET_MSG(sts);
            goto exit_here;
        }
        
        nDecSurfNum = DecResponse.NumFrameActual;

        std::cout << "\t\t. Decode Surface Actual Number: " << nDecSurfNum << std::endl;

        // this surface will be used when decodes encoded stream
        pmfxDecSurfaces[nLoop] = new mfxFrameSurface1 * [nDecSurfNum];

        if(!pmfxDecSurfaces[nLoop] )
        {
            MSDK_PRINT_RET_MSG(MFX_ERR_MEMORY_ALLOC);            
            goto exit_here;
        }
        
        for (int i = 0; i < nDecSurfNum; i++)
        {
            pmfxDecSurfaces[nLoop][i] = new mfxFrameSurface1;
            memset(pmfxDecSurfaces[nLoop][i], 0, sizeof(mfxFrameSurface1));
            memcpy(&(pmfxDecSurfaces[nLoop][i]->Info), &(DecParams.mfx.FrameInfo), sizeof(mfxFrameInfo));

            // external allocator used - provide just MemIds
            pmfxDecSurfaces[nLoop][i]->Data.MemId = DecResponse.mids[i];
        }

        // [VPP_In]
        // memory allocation
        mfxFrameAllocResponse VPP_In_Response = { 0 };
        memcpy(&VPPRequest[0].Info, &(VPPParams.vpp.In), sizeof(mfxFrameInfo)); // allocate VPP input frame information

        sts = mfxAllocator[nLoop].Alloc(mfxAllocator[nLoop].pthis, &(VPPRequest[0]), &VPP_In_Response);
        if(MFX_ERR_NONE > sts)
        {
            MSDK_PRINT_RET_MSG(sts);
            return 1;
        }

        mfxU16 nVPP_In_SurfNum = VPP_In_Response.NumFrameActual;

        std::cout << "\t\t. VPP In Surface Actual Number: " << nVPP_In_SurfNum << std::endl;

        // this surface will contain decoded frame
        pmfxVPP_In_Surfaces[nLoop] = new mfxFrameSurface1 * [nVPP_In_SurfNum];

        if(!pmfxVPP_In_Surfaces[nLoop])
        {
            MSDK_PRINT_RET_MSG(MFX_ERR_MEMORY_ALLOC);            
            return 1;
        }
        
        for (int i = 0; i < nVPP_In_SurfNum; i++)
        {
            pmfxVPP_In_Surfaces[nLoop][i] = new mfxFrameSurface1;
            memset(pmfxVPP_In_Surfaces[nLoop][i], 0, sizeof(mfxFrameSurface1));
            memcpy(&(pmfxVPP_In_Surfaces[nLoop][i]->Info), &(VPPParams.mfx.FrameInfo), sizeof(mfxFrameInfo));

            // external allocator used - provide just MemIds
            pmfxVPP_In_Surfaces[nLoop][i]->Data.MemId = VPP_In_Response.mids[i];
        }

        // [VPP_Out]
        // memory allocation
        mfxFrameAllocResponse VPP_Out_Response = { 0 };
        memcpy(&VPPRequest[1].Info, &(VPPParams.vpp.Out), sizeof(mfxFrameInfo));    // allocate VPP output frame information

        sts = mfxAllocator[nLoop].Alloc(mfxAllocator[nLoop].pthis, &(VPPRequest[1]), &VPP_Out_Response);

        if(MFX_ERR_NONE > sts)
        {
            MSDK_PRINT_RET_MSG(sts);
            return 1;
        }

        mfxU16 nVPP_Out_SurfNum = VPP_Out_Response.NumFrameActual;

        std::cout << "\t\t. VPP Out Surface Actual Number: " << nVPP_Out_SurfNum << std::endl;

        // this surface will contain resized frame (video processed)
        pmfxVPP_Out_Surfaces[nLoop] = new mfxFrameSurface1 * [nVPP_Out_SurfNum];

        if(!pmfxVPP_Out_Surfaces[nLoop])
        {
            MSDK_PRINT_RET_MSG(MFX_ERR_MEMORY_ALLOC);            
            return 1;
        }

        for (int i = 0; i < nVPP_Out_SurfNum; i++)
        {
            pmfxVPP_Out_Surfaces[nLoop][i] = new mfxFrameSurface1;
            memset(pmfxVPP_Out_Surfaces[nLoop][i], 0, sizeof(mfxFrameSurface1));
            memcpy(&(pmfxVPP_Out_Surfaces[nLoop][i]->Info), &(VPPParams.vpp.Out), sizeof(mfxFrameInfo));

            // external allocator used - provide just MemIds
            pmfxVPP_Out_Surfaces[nLoop][i]->Data.MemId = VPP_Out_Response.mids[i];
        }

        // Initialize the Media SDK decoder
        std::cout << "\t. Init Intel Media SDK Decoder" << std::endl;

        sts = pmfxDEC->Init(&DecParams);
        MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
        
        if(MFX_ERR_NONE > sts)
        {
            MSDK_PRINT_RET_MSG(sts);
            goto exit_here;
        }

        std::cout << "\t. Done decoder Init" << std::endl;

        // Initialize Media SDK VPP
        std::cout << "\t. Init Intel Media SDK VPP" << std::endl;

        sts = pmfxVPP->Init(&VPPParams);
        printf("VPP ext buffer=%d\r\n",VPPParams.NumExtParam );
        MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        std::cout << "\t. Done VPP Init" << std::endl;

        std::cout << "> Done Intel Media SDK session, decoder, VPP initialization." << std::endl;
        std::cout << std::endl;

       // Start decoding the frames from the stream
        for (mfxU16 i = 0; i < nDecSurfNum; i++)
        {
             std::cout<<"dec:handle (%d): "<<pmfxDecSurfaces[nLoop][i]<<std::endl;
        }


        for (mfxU16 i = 0; i < nVPP_In_SurfNum; i++)
        {
             std::cout<<"vpp:handle: "<<pmfxVPP_In_Surfaces[nLoop][i]<<std::endl;
        }

        //int decOutputFile = open("./dump.nv12", O_APPEND | O_CREAT, S_IRWXO);

        DecThreadConfig *pDecThreadConfig = new DecThreadConfig();
        memset(pDecThreadConfig, 0, sizeof(DecThreadConfig));
        pDecThreadConfig->totalDecNum            = FLAGS_fr;
        pDecThreadConfig->f_i                    = f_i[nLoop];
        pDecThreadConfig->pmfxDEC                = pmfxDEC;
        pDecThreadConfig->pmfxVPP                = pmfxVPP;
        pDecThreadConfig->nDecSurfNum            = nDecSurfNum;
        pDecThreadConfig->nVPP_In_SurfNum        = nVPP_In_SurfNum;
        pDecThreadConfig->nVPP_Out_SurfNum       = nVPP_Out_SurfNum;
        pDecThreadConfig->pmfxDecSurfaces        = pmfxDecSurfaces[nLoop];
        pDecThreadConfig->pmfxVPP_In_Surfaces    = pmfxVPP_In_Surfaces[nLoop];
        pDecThreadConfig->pmfxVPP_Out_Surfaces   = pmfxVPP_Out_Surfaces[nLoop];
        pDecThreadConfig->pmfxBS                 = &mfxBS[nLoop];
        pDecThreadConfig->pmfxSession            = &mfxSession[nLoop];
        pDecThreadConfig->pmfxAllocator          = &mfxAllocator[nLoop];
        pDecThreadConfig->nChannel               = nLoop;
        pDecThreadConfig->nFPS                   = 1;//30/pDecThreadConfig->nFPS;
        pDecThreadConfig->bStartCount            = false;
        pDecThreadConfig->nFrameProcessed        = 0;
        pDecThreadConfig->dpipe                  = dpipe[nLoop];
#ifdef TEST_KCF_TRACK_WITH_GPU
		pDecThreadConfig->dKCFpipe               = dKCFpipe[nLoop];
#endif
        vpDecThradConfig.push_back(pDecThreadConfig);

        pthread_t threadid;
        pthread_create(&threadid, NULL, DecodeThreadFunc, (void *)(pDecThreadConfig));

        vDecThreads.push_back(threadid) ; 
#ifdef TEST_KCF_TRACK_WITH_GPU
        //Create a KCF Tracking Thread for each channel 
        TrackerThreadConfig *pTrackerThreadConfig = new TrackerThreadConfig();
        memset(pTrackerThreadConfig, 0, sizeof(TrackerThreadConfig));
        pTrackerThreadConfig->totalDecNum            = FLAGS_fr;
        pTrackerThreadConfig->pmfxDEC                = pmfxDEC;
        pTrackerThreadConfig->pmfxVPP                = pmfxVPP;
        pTrackerThreadConfig->nDecSurfNum            = nDecSurfNum;
        pTrackerThreadConfig->nVPP_In_SurfNum        = nVPP_In_SurfNum;
        pTrackerThreadConfig->nVPP_Out_SurfNum       = nVPP_Out_SurfNum;
        pTrackerThreadConfig->pmfxDecSurfaces        = pmfxDecSurfaces[nLoop];
        pTrackerThreadConfig->pmfxVPP_In_Surfaces    = pmfxVPP_In_Surfaces[nLoop];
        pTrackerThreadConfig->pmfxVPP_Out_Surfaces   = pmfxVPP_Out_Surfaces[nLoop];
        pTrackerThreadConfig->pmfxSession            = &mfxSession[nLoop];
        pTrackerThreadConfig->pmfxAllocator          = &mfxAllocator[nLoop];
        pTrackerThreadConfig->nChannel               = nLoop;
        pTrackerThreadConfig->dpipe                  = dKCFpipe[nLoop];
        pTrackerThreadConfig->width                   = DecParams.mfx.FrameInfo.CropW;
        pTrackerThreadConfig->height                  = DecParams.mfx.FrameInfo.CropH;
        
        vpTrackerThreadConfig.push_back(pTrackerThreadConfig);

        //pthread_t threadid;
        pthread_create(&threadid, NULL, TrackerThreadFunc, (void *)(pTrackerThreadConfig));

        vKCFTrackerThreads.push_back(threadid);
        //vKCFTrackerThreads
#endif

    }
 #ifdef TEST_KCF_TRACK_WITH_GPU
	for(int i=0; i< NUM_OF_CHANNELS; i++)
    {
        sem_init(&gNewTrackTaskAvaiable[i],0,0);
		sem_init(&gDetectResultAvaiable[i],0,0);
    }
 #endif

  // Initialize scheduler thread
    {
        sem_init(&gNewtaskAvaiable,0,0);
        ScheduleThreadConfig *pScheduleThreadConfig = new ScheduleThreadConfig();
        memset(pScheduleThreadConfig, 0, sizeof(pScheduleThreadConfig));
        pScheduleThreadConfig->gNet_input_width       = gNet_input_width;
        pScheduleThreadConfig->gNet_input_height      = gNet_input_height;
        pScheduleThreadConfig->pvdpipe                = &vdpipe;
        pScheduleThreadConfig->bTerminated            = false;

        vpScheduleThreadConfig.push_back(pScheduleThreadConfig);

        pthread_t threadid;
        pthread_create(&threadid, NULL, ScheduleThreadFunc, (void *)(pScheduleThreadConfig));

        vScheduleThreads.push_back(threadid) ; 

   // vInferThreads.push_back(  std::thread(ScheduleThreadFunc,
   //                          (void *)(pScheduleThreadConfig)
   //                         ) );   
    }
    for(int nLoop=0; nLoop< NUM_OF_GPU_INFER; nLoop++)
    {
        InferThreadConfig *pInferThreadConfig = new InferThreadConfig();
        memset(pInferThreadConfig, 0, sizeof(InferThreadConfig));
        pInferThreadConfig->nChannel           = nLoop;// GPU instance ID;
        pInferThreadConfig->bStartCount = true;
        pInferThreadConfig->bTerminated = false;
        pthread_t threadid;
        vpInferThreadConfig.push_back(pInferThreadConfig);
	    /*vInferThreads.push_back(  std::thread(InferThreadFunc,
	                             (void *)(pInferThreadConfig)
	                            ) );  */  
        pthread_create(&threadid, NULL, InferThreadFunc, (void *)(pInferThreadConfig));	
        vInferThreads.push_back(threadid) ; 
    }

    // set the handler of ctrl-c
    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = sigint_hanlder;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);

    pause();
    
    grunning=false;
    
    for(int nLoop=0; nLoop < NUM_OF_GPU_INFER; nLoop++){
        sem_post(&gsemtInfer[nLoop]);
    }    
    sem_post(&gNewtaskAvaiable);
    sem_post(&g_semtshow);
    for (auto& th : vAssistThreads) {
        pthread_join(th,NULL);
    }

    for (auto& th : vInferThreads) {
        pthread_join(th,NULL);
    }

    for (auto& th : vScheduleThreads) {
        pthread_join(th,NULL);
    }

    for (auto& th : vDecThreads) {
        pthread_join(th,NULL);
    }
 
    std::cout<<" All thread is termined " <<std::endl;
    // Report performance counts
    {
       //TODO: add summary

    }

exit_here:

    for(int nLoop=0; nLoop< FLAGS_c; nLoop++)
    {
        fclose(f_i[nLoop]);
        //fclose(f_o);

        MSDK_SAFE_DELETE_ARRAY(pmfxDecSurfaces[nLoop]);
        MSDK_SAFE_DELETE_ARRAY(pmfxVPP_In_Surfaces[nLoop]);
        MSDK_SAFE_DELETE_ARRAY(pmfxVPP_Out_Surfaces[nLoop]);
        MSDK_SAFE_DELETE_ARRAY(mfxBS[nLoop].Data);
    }

    sem_destroy(&g_semtshow);
    sem_destroy(&gNewtaskAvaiable);
    for(int nLoop=0; nLoop < NUM_OF_GPU_INFER; nLoop++){
        sem_destroy(&gsemtInfer[nLoop]);
        pthread_mutex_destroy(&mutexinfer[nLoop]);  
    }    
    pthread_mutex_destroy(&mutexshow);  
    for(int nLoop=0; nLoop< FLAGS_c; nLoop++)
    {
        printf("Deleting %d dpipe\n", nLoop);
        delete dpipe[nLoop];
    }
 
    std::cout << "> Complete execution !";

    fflush(stdout);
    return 0;
}

/**
* \brief This function show a help message
*/
void App_ShowUsage()
{
    std::cout << std::endl;
    std::cout << "[usage]" << std::endl;
    std::cout << "\video_analytics_example [option]" << std::endl;
    std::cout << "\toptions:" << std::endl;
    std::cout << std::endl;
    std::cout << "\t\t-h           " << help_message << std::endl;
    std::cout << "\t\t-i..........." << image_message << std::endl;
    std::cout << "\t\t-m <path>    " << model_message << std::endl;
    std::cout << "\t\t-l <path>    " << labels_message << std::endl;
    std::cout << "\t\t-d <device>  " << target_device_message << std::endl;
    std::cout << "\t\t-c <steams>  " << channels_message << std::endl;
    std::cout << "\t\t-show        " << show_message << std::endl;
    std::cout << "\t\t-batch <val> " << batch_message << std::endl;
    std::cout << "\t\t-dec_postproc <val>     " << dec_postproc_message << std::endl;
    std::cout << "\t\t-pi     " << performance_inference_message<< std::endl;
    std::cout << "\t\t-pd     " << performance_decode_message << std::endl;
    std::cout << "\t\t-pl     " << pipeline_latency_message << std::endl;
    std::cout << "\t\t-pv     " << perf_details_message << std::endl;
    std::cout << "\t\t-infer  <val>    " << inference_message << std::endl;
  
}


mfxStatus WriteRawFrameToMemory(mfxFrameSurface1* pSurface, int dest_w, int dest_h, unsigned char* dest, mfxU32 fourCC)
{
    mfxFrameInfo* pInfo = &pSurface->Info;
    mfxFrameData* pData = &pSurface->Data;
    mfxU32 i, j, h, w;
    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 iYsize, ipos;


    if(fourCC == MFX_FOURCC_RGBP)
    {
        mfxU8* ptr;

        if (pInfo->CropH > 0 && pInfo->CropW > 0)
        {
            w = pInfo->CropW;
            h = pInfo->CropH;
        }
        else
        {
            w = pInfo->Width;
            h = pInfo->Height;
        }

        mfxU8 *pTemp = dest;
        ptr   = pData->B + (pInfo->CropX ) + (pInfo->CropY ) * pData->Pitch;

        for (i = 0; i < dest_h; i++)
        {
           memcpy(pTemp + i*dest_w, ptr + i*pData->Pitch, dest_w);
        }


        ptr	= pData->G + (pInfo->CropX ) + (pInfo->CropY ) * pData->Pitch;
        pTemp = dest + dest_w*dest_h;
        for(i = 0; i < dest_h; i++)
        {
           memcpy(pTemp  + i*dest_w, ptr + i*pData->Pitch, dest_w);
        }

        ptr	= pData->R + (pInfo->CropX ) + (pInfo->CropY ) * pData->Pitch;
        pTemp = dest + 2*dest_w*dest_h;
        for(i = 0; i < dest_h; i++)
        {
            memcpy(pTemp  + i*dest_w, ptr + i*pData->Pitch, dest_w);
        }

        //for(i = 0; i < h; i++)
        //memcpy(dest + i*w*4, ptr + i * pData->Pitch, w*4);
    }
    else if(fourCC == MFX_FOURCC_RGB4)
    {
        mfxU8* ptr;

        if (pInfo->CropH > 0 && pInfo->CropW > 0)
        {
            w = pInfo->CropW;
            h = pInfo->CropH;
        }
        else
        {
            w = pInfo->Width;
            h = pInfo->Height;
        }

        ptr = MSDK_MIN( MSDK_MIN(pData->R, pData->G), pData->B);
        ptr = ptr + pInfo->CropX + pInfo->CropY * pData->Pitch;

        for(i = 0; i < dest_h; i++)
            memcpy(dest + i*dest_w*4, ptr + i * pData->Pitch, dest_w*4);

        //for(i = 0; i < h; i++)
        //memcpy(dest + i*w*4, ptr + i * pData->Pitch, w*4);
    }
    else if(fourCC == MFX_FOURCC_NV12)
    {
        iYsize = pInfo->CropH * pInfo->CropW;

        for (i = 0; i < pInfo->CropH; i++)
            memcpy(dest + i * (pInfo->CropW), pData->Y + (pInfo->CropY * pData->Pitch + pInfo->CropX) + i * pData->Pitch, pInfo->CropW);

        ipos = iYsize;

        h = pInfo->CropH / 2;
        w = pInfo->CropW;

        for (i = 0; i < h; i++)
            for (j = 0; j < w; j += 2)
                memcpy(dest+ipos++, pData->UV + (pInfo->CropY * pData->Pitch / 2 + pInfo->CropX) + i * pData->Pitch + j, 1);

        for (i = 0; i < h; i++)
            for (j = 1; j < w; j += 2)
                memcpy(dest+ipos++, pData->UV + (pInfo->CropY * pData->Pitch / 2 + pInfo->CropX)+ i * pData->Pitch + j, 1);


    }

    return sts;
}

cv::Mat createMat(unsigned char *rawData, unsigned int dimX, unsigned int dimY)
{
    // No need to allocate outputMat here
    cv::Mat outputMat;

    // Build headers on your raw data
    cv::Mat channelR(dimY, dimX, CV_8UC1, rawData);
    cv::Mat channelG(dimY, dimX, CV_8UC1, rawData + dimX * dimY);
    cv::Mat channelB(dimY, dimX, CV_8UC1, rawData + 2 * dimX * dimY);

    // Invert channels, 
    // don't copy data, just the matrix headers
    std::vector<cv::Mat> channels{ channelB, channelG, channelR };

    // Create the output matrix
    merge(channels, outputMat);

    return outputMat;
}


