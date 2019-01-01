/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                           License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2010-2013, University of Nizhny Novgorod, all rights reserved.
// Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of the copyright holders may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the Intel Corporation or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/


//Modified from latentsvm module's "lsvmc_featurepyramid.cpp".

//#include "precomp.hpp"
//#include "_lsvmc_latentsvm.h"
//#include "_lsvmc_resizeimg.h"


#ifndef _KCFTRACKER_HEADERS
#include "kcftracker.hpp"
#include "fhog.hpp"
#endif
#include <chrono>
using namespace std;
#include "fhog.hpp"
#ifdef HAVE_TBB
#include <tbb/tbb.h>
#include "tbb/parallel_for.h"
#include "tbb/blocked_range.h"
#endif
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>
#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

using namespace std;
/*
// Getting feature map for the selected subimage
//
// API
// int getFeatureMaps(const IplImage * image, const int k, featureMap **map);
// INPUT
// image             - selected subimage
// k                 - size of cells
// OUTPUT
// map               - feature map
// RESULT
// Error status
*/

static unsigned int nframes=0;
static unsigned int bCapture = false;
int getFeatureMaps(const IplImage* image, const int k, CvLSVMFeatureMapCaskade **map)
{
    int sizeX, sizeY;
    int p, px, stringSize;
    int height, width, numChannels;
    int i, j, kk, c, ii, jj, d;
    float  * datadx, * datady;
    
    int   ch; 
    float magnitude, x, y, tx, ty;
    
    IplImage * dx, * dy;
    int *nearest;
    float *w, a_x, b_x;

    float kernel[3] = {-1.f, 0.f, 1.f};
    CvMat kernel_dx = cvMat(1, 3, CV_32F, kernel);
    CvMat kernel_dy = cvMat(3, 1, CV_32F, kernel);

    float * r;
    int   * alfa;
    
    float boundary_x[NUM_SECTOR + 1];
    float boundary_y[NUM_SECTOR + 1];
    float max, dotProd;
    int   maxi;

    height = image->height;
    width  = image->width ;
    float arg_vector;
    for(i = 0; i <= NUM_SECTOR; i++)
    {
        arg_vector    = ( (float) i ) * ( (float)(PI) / (float)(NUM_SECTOR) );
        boundary_x[i] = cosf(arg_vector);
        boundary_y[i] = sinf(arg_vector);
    }/*for(i = 0; i <= NUM_SECTOR; i++) */

    r    = (float *)malloc( sizeof(float) * (width * height));
    alfa = (int   *)malloc( sizeof(int  ) * (width * height * 2));
    nearest = (int  *)malloc(sizeof(int  ) *  k);
    w       = (float*)malloc(sizeof(float) * (k * 2));
    for(i = 0; i < k / 2; i++)
    {
        nearest[i] = -1;
    }/*for(i = 0; i < k / 2; i++)*/
    for(i = k / 2; i < k; i++)
    {
        nearest[i] = 1;
    }/*for(i = k / 2; i < k; i++)*/

    for(j = 0; j < k / 2; j++)
    {
        b_x = k / 2 + j + 0.5f;
        a_x = k / 2 - j - 0.5f;
        w[j * 2    ] = 1.0f/a_x * ((a_x * b_x) / ( a_x + b_x)); 
        w[j * 2 + 1] = 1.0f/b_x * ((a_x * b_x) / ( a_x + b_x));  
    }/*for(j = 0; j < k / 2; j++)*/
    for(j = k / 2; j < k; j++)
    {
        a_x = j - k / 2 + 0.5f;
        b_x =-j + k / 2 - 0.5f + k;
        w[j * 2    ] = 1.0f/a_x * ((a_x * b_x) / ( a_x + b_x)); 
        w[j * 2 + 1] = 1.0f/b_x * ((a_x * b_x) / ( a_x + b_x));  
    }/*for(j = k / 2; j < k; j++)*/


    	
#if 1
    IplImage *blue_img = cvCreateImage( cvGetSize(image),IPL_DEPTH_32F,1 );
    IplImage *green_img = cvCreateImage( cvGetSize(image),IPL_DEPTH_32F,1 );
    IplImage *red_img = cvCreateImage(cvGetSize(image),IPL_DEPTH_32F,1 );
    IplImage *all_img = cvCreateImage( cvGetSize(image),IPL_DEPTH_32F,3 );


    IplImage *blue_img0 = cvCreateImage( cvGetSize(image),IPL_DEPTH_8U,1 );
    IplImage *green_img0 = cvCreateImage( cvGetSize(image),IPL_DEPTH_8U,1 );
    IplImage *red_img0 = cvCreateImage(cvGetSize(image),IPL_DEPTH_8U,1 );
 #endif
        CvSize size(height+2, width+2);
#if 1
    // three channels
    nframes++;
    if(nframes == 300){
       bCapture = true;
    }
    if(nframes > 300){
       bCapture = false;
    }
    std::cout<<" num of channels:"<< image->nChannels << "  kernel size="<< k <<std::endl;
    std::cout<<" height: "<< height << "  width="<< width <<std::endl;

    if(bCapture){
        char szBuffer[250]={0}; 
        sprintf(szBuffer, "width=%d, height=%d", width, height);
	FILE *fpTemp = fopen("./dump.info", "w+t");
	if(NULL!= fpTemp)
	{
	       fwrite(szBuffer, 1, strlen(szBuffer), fpTemp);
	       fclose(fpTemp);
	}
    }
    //DUMP INPUT
    unsigned char * img_origR = new unsigned char[height * width];
    unsigned char * img_origG = new unsigned char[height * width];
    unsigned char * img_origB = new unsigned char[height * width];
    unsigned char *pRawImage;
    for (int x = 0; x < height; ++x) {
        pRawImage = (unsigned char*)(image->imageData + image->widthStep * (x));
        for (int y = 0; y < width; ++y) {
           img_origR[x*width + y] = pRawImage[(y)*3 + 0];
           img_origG[x*width + y] = pRawImage[(y)*3 + 1]; 
           img_origB[x*width + y] = pRawImage[(y)*3 + 2];
        }
    }
    if(bCapture){
	    FILE *fpR = fopen("./R.data", "w+b");
	    if(NULL!= fpR)
	    {
	       fwrite(img_origR, 1, height*width, fpR);
	       fclose(fpR);
	    }
	    FILE *fpG = fopen("./G.data", "w+b");
	    if(NULL!= fpG)
	    {
	       fwrite(img_origG, 1, height*width, fpG);
	       fclose(fpG);
	    }
	    FILE *fpB = fopen("./B.data", "w+b");
	    if(NULL!= fpB)
	    {
	       fwrite(img_origB, 1, height*width, fpB);
	       fclose(fpB);
	    }
    }
    //END OF DUMP INPUT
    float * img_origR1 = new float[(height+2)*(width+2)];
    float * img_origG1 = new float[(height+2)*(width+2)];
    float * img_origB1 = new float[(height+2)*(width+2)];
 
    for (int x = 1; x < height+1; ++x) {
        pRawImage = (unsigned char*)(image->imageData + image->widthStep * (x-1));
        for (int y = 1; y < width+1; ++y) {
           //img_origR1[x*(width+2) + y] = image->imageData[(x-1)*(width*3) + 3*(y-1)];
           //img_origG1[x*(width+2) + y] = image->imageData[(x-1)*(width*3) + 3*(y-1) + 1]; 
           //img_origB1[x*(width+2) + y] = image->imageData[(x-1)*(width*3) + 3*(y-1) + 2];
           img_origR1[x*(width+2) + y] = pRawImage[(y-1)*3 + 0];
           img_origG1[x*(width+2) + y] = pRawImage[(y-1)*3 + 1]; 
           img_origB1[x*(width+2) + y] = pRawImage[(y-1)*3 + 2];
        }
    }


    // Padding
    for (int x = 0; x < 1; ++x) {
        for (int y = 1; y < width+1; ++y) {
           img_origR1[ y] = img_origR1[(x+1)*(width+2) + y];
           img_origG1[ y] = img_origG1[(x+1)*(width+2) + y]; 
           img_origB1[ y] = img_origB1[(x+1)*(width+2) + y];
        }
    }
    for (int x = height+1; x < height+2; ++x) {
        for (int y = 0; y < width+1; ++y) {
           img_origR1[x*(width+2) + y] = img_origR1[(x-1)*(width+2) + y];
           img_origG1[x*(width+2) + y] = img_origG1[(x-1)*(width+2) + y]; 
           img_origB1[x*(width+2) + y] = img_origB1[(x-1)*(width+2) + y];
        }
    }

    for (int x = 0; x < height+1; ++x) {
        for (int y = 0; y < 1; ++y) {
           img_origR1[x*(width+2) + y] = img_origR1[x*(width+2) + y+1];
           img_origG1[x*(width+2) + y] = img_origG1[x*(width+2) + y+1]; 
           img_origB1[x*(width+2) + y] = img_origB1[x*(width+2) + y+1];
        }
    }
    for (int x = 0; x < height+1; ++x) {
        for (int y = width+1; y < width+2; ++y) {
           img_origR1[x*(width+2) + y] = img_origR1[x*(width+2) + y-1];
           img_origG1[x*(width+2) + y] = img_origG1[x*(width+2) + y-1]; 
           img_origB1[x*(width+2) + y] = img_origB1[x*(width+2) + y-1];
        }
    }

    img_origR1[0] = img_origR1[width+2 + 1];
    img_origG1[0] = img_origG1[width+2 + 1];
    img_origB1[0] = img_origB1[width+2 + 1];

    img_origR1[width+1] = img_origR1[2*(width+2) -2];
    img_origG1[width+1] = img_origG1[2*(width+2) -2];
    img_origB1[width+1] = img_origB1[2*(width+2) -2];

    img_origR1[(width+2)*(height+1)] = img_origR1[(width+2)*(height) + 1];
    img_origG1[(width+2)*(height+1)] = img_origG1[(width+2)*(height) + 1];
    img_origB1[(width+2)*(height+1)] = img_origB1[(width+2)*(height) + 1];

    img_origR1[(width+2)*(height+1) + width+1] = img_origR1[(width+2)*(height) + width];
    img_origG1[(width+2)*(height+1) + width+1] = img_origG1[(width+2)*(height) + width];
    img_origB1[(width+2)*(height+1) + width+1] = img_origB1[(width+2)*(height) + width];
    std::chrono::high_resolution_clock::time_point tmStart;
    std::chrono::high_resolution_clock::time_point tmEnd;
    tmStart = std::chrono::high_resolution_clock::now();
    vector<float> R;
    for(int row=1; row< height+1; row++)
      for(int col=1; col< width+1; col++)
       {
             float a=0.0f;
             for(int m=0; m< 3; m++) 
              {
                  a+=kernel[m]*img_origR1[row*(width+2)+col + m -1];
              }
             R.push_back(a);
       }
    vector<float> G;
    for(int row=1; row< height+1; row++)
      for(int col=1; col< width+1; col++)
       {
             float a=0.0f;
             for(int m=0; m< 3; m++) 
              {
                  a+=kernel[m]*img_origG1[row*(width+2)+col + m -1];
              }
             G.push_back(a);
       }
    vector<float> B;
    for(int row=1; row< height+1; row++){
      for(int col=1; col< width+1; col++)
       {
             float a=0.0f;
             for(int m=0; m< 3; m++) 
              { 
                  a+=kernel[m]*img_origB1[row*(width+2)+col + m -1];
              }
 //printf("ddd%f, %f, %f,  a=%f ", img_origB1[row*(width+2)+col  -1],img_origB1[row*(width+2)+col  ],img_origB1[row*(width+2)+col  +1],a);

             B.push_back(a);
       }
     //  printf("\r\n");
    }

 

    vector<float> R1;
    for(int row=1; row< height+1; row++)
      for(int col=1; col< width+1; col++)
       {
             float a=0.0f;
             for(int m=0; m< 3; m++) 
              {
                  a+=kernel[m]*img_origR1[(row+m-1)*(width+2)+col ];
              }
             R1.push_back(a);
       }
    vector<float> G1;
    for(int row=1; row< height+1; row++)
      for(int col=1; col< width+1; col++)
       {
             float a=0.0f;
             for(int m=0; m< 3; m++) 
              {
                  a+=kernel[m]*img_origG1[(row+m-1)*(width+2)+col ];
              }
             G1.push_back(a);
       }
    vector<float> B1;
    for(int row=1; row< height+1; row++)
      for(int col=1; col< width+1; col++)
       {
             float a=0.0f;
             for(int m=0; m< 3; m++) 
              {
                  a+=kernel[m]*img_origB1[(row+m-1)*(width+2)+col];
              }
             B1.push_back(a);
       }

#endif

    numChannels = image->nChannels;

    dx    = cvCreateImage(cvSize(image->width, image->height), 
                          IPL_DEPTH_32F, 3);

    dy    = cvCreateImage(cvSize(image->width, image->height), 
                          IPL_DEPTH_32F, 3);

    sizeX = width  / k;
    sizeY = height / k;
    px    = 3 * NUM_SECTOR; 
    p     = px;
    stringSize = sizeX * p;
    allocFeatureMapObject(map, sizeX, sizeY, p);
#if 0
   unsigned char *pData;
   printf("dump raw  R image data 8x8\r\n");
   for(j = 0; j < 8 ; j++)
    {
        pData = (unsigned char*)(image->imageData + image->widthStep * j);
        for(i = width-8; i < width ; i++)
        {
          printf("%d ", pData[i*3 +0]);
        }
          printf("\r\n");
     }
   printf("dump raw G image data 8x8\r\n");
   for(j = 0; j < 8 ; j++)
    {
        pData = (unsigned char*)(image->imageData + image->widthStep * j);
          for(i = width-8; i < width ; i++)
        {
          printf("%d ", pData[i*3 +1]);
        }
          printf("\r\n");
     }

   printf("dump raw B image data 8x8\r\n");
   for(j = 0; j < 8 ; j++)
    {
        pData = (unsigned char*)(image->imageData + image->widthStep * j);
          for(i = width-8; i < width ; i++)
        {
          printf("%d ", pData[i*3 +2]);
        }
          printf("\r\n");
     }
   float *pTemp;
   printf("dump float B channel\r\n");
   for(j =1; j < 9 ; j++)
    {

          for(i = width-6; i < width+2 ; i++)
        {
          printf("%d ", int(img_origB1[j*(width+2) + i]));
        }
              printf("\r\n");   
     }
#endif
    //Ben: Convolution 1*3 or 3*1
    //cvFilter2D(image, all_img, &kernel_dy, cvPoint(-1, 0));
    //cvFilter2D(image, dy, &kernel_dy, cvPoint(0, -1));      
#if 0    
    float *pDataRe;
    printf("dump raw all_img from SW data 8x8\r\n");
    for(j = 0; j < height ; j++)
    {
        pDataRe = (float *)(all_img->imageData + all_img->widthStep * j);
       
         for(i = width-8; i < width ; i++)
        {
          printf("%f ", pDataRe[i*3+2]);
        }
          printf("\r\n");
    }
#endif
    float* r0;
    for(j = 0; j < height; j++)
    {
        r0 = (float*)(dx->imageData + dx->widthStep * j);
        for(i = 0; i < width; i++)
        {
          r0[i*3+0 ] = R[j*(width) + i];
          r0[i*3+1] =  G[j*(width) + i];
          r0[i*3+2] =  B[j*(width) + i];
        }
     }


    for(j = 0; j < height; j++)
    {
        r0 = (float*)(dy->imageData + dy->widthStep * j);
    
        for(i = 0; i < width; i++)
        {
          r0[i*3+0 ] = R1[j*(width) + i];
          r0[i*3+1]  = G1[j*(width)  + i];
          r0[i*3+2]  = B1[j*(width)  + i];
        }
     }

#if 0
   float *pDataRe2;
   float *pDataRe3;
   printf("dump raw dx from SW data 8x8\r\n");
   for(j = 0; j < height ; j++)
   {
        pDataRe2 = (float *)(dx->imageData + dx->widthStep * j);
       // pDataRe3 = (float *)(all_img->imageData + all_img->widthStep * j);
          for(i = width-8; i < width ; i++)
        {
          printf("%f ", pDataRe2[i*3+2]);
        }
          printf("\r\n");
   }

	//cvSplit(image,blue_img,green_img,red_img,0);
	//cvMerge(red_img,green_img,blue_img,0,dx);                                                                                                                                                                                                                                                                                                                                
	cvNamedWindow("hello",CV_WINDOW_AUTOSIZE);
	cvShowImage("dx image",all_img);
	cvShowImage("yyhello",dy);
	cvWaitKey(0);
#endif

    // DUMP output of convolution
    float * img_convR = new float[height * width];
    float * img_convG = new float[height * width];
    float * img_convB = new float[height * width];
   
    for (int x = 0; x < height; ++x) {
        pRawImage = (unsigned char*)(dx->imageData + dx->widthStep * (x));
        for (int y = 0; y < width; ++y) {
           img_convR[x*(width) + y] = pRawImage[(y)*3 + 0];
           img_convG[x*(width) + y] = pRawImage[(y)*3 + 1]; 
           img_convB[x*(width) + y] = pRawImage[(y)*3 + 2];
        }
    }
 if(bCapture){
    FILE *fpRconv = fopen("./Rconvdx.data", "w+b");
    if(NULL!= fpRconv)
    {
       fwrite(img_convR, sizeof(float), height*width, fpRconv);
       fclose(fpRconv);
    }
    FILE *fpGconv = fopen("./Gconvdx.data", "w+b");
    if(NULL!= fpGconv)
    {
       fwrite(img_convG, sizeof(float), height*width, fpGconv);
       fclose(fpGconv);
    }
    FILE *fpBconv = fopen("./Bconvdx.data", "w+b");
    if(NULL!= fpBconv)
    {
       fwrite(img_convB, sizeof(float), height*width, fpBconv);
       fclose(fpBconv);
    }
}
   for (int x = 0; x < height; ++x) {
        pRawImage = (unsigned char*)(dy->imageData + dy->widthStep * (x));
        for (int y = 0; y < width; ++y) {
           img_convR[x*(width) + y] = pRawImage[(y)*3 + 0];
           img_convG[x*(width) + y] = pRawImage[(y)*3 + 1]; 
           img_convB[x*(width) + y] = pRawImage[(y)*3 + 2];
        }
    }
 if(bCapture){ 
   FILE * fpRconv = fopen("./Rconvdy.data", "w+b");
    if(NULL!= fpRconv)
    {
       fwrite(img_convR, sizeof(float), height*width, fpRconv);
       fclose(fpRconv);
    }
    FILE *fpGconv = fopen("./Gconvdy.data", "w+b");
    if(NULL!= fpGconv)
    {
       fwrite(img_convG, sizeof(float), height*width, fpGconv);
       fclose(fpGconv);
    }
   FILE * fpBconv = fopen("./Bconvdy.data", "w+b");
    if(NULL!= fpBconv)
    {
       fwrite(img_convB, sizeof(float), height*width, fpBconv);
       fclose(fpBconv);
    }
}

  //  std::cout<< "  >getFeatures part2 width:" << width << "height" << height<<"num of channel"<<numChannels <<" k="<<k<<" widthStep="<<dx->widthStep<<" sizeY="<<sizeY<<" sizeX="<<sizeX<<std::endl;
    for(j = 1; j < height - 1; j++)
    {
        datadx = (float*)(dx->imageData + dx->widthStep * j);
        datady = (float*)(dy->imageData + dy->widthStep * j);
        for(i = 1; i < width - 1; i++)
        {
            c = 0;
            x = (datadx[i * numChannels + c]);
            y = (datady[i * numChannels + c]);
            // Ben: Does MDF support sqrtf?
            r[j * width + i] =sqrtf(x * x + y * y);
            for(ch = 1; ch < numChannels; ch++)
            {
                tx = (datadx[i * numChannels + ch]);
                ty = (datady[i * numChannels + ch]);
                magnitude = sqrtf(tx * tx + ty * ty);
                if(magnitude > r[j * width + i])
                {
                    r[j * width + i] = magnitude;
                    c = ch;
                    x = tx;
                    y = ty;
                }
            }/*for(ch = 1; ch < numChannels; ch++)*/
            
            max  = boundary_x[0] * x + boundary_y[0] * y;
            maxi = 0;
            for (kk = 0; kk < NUM_SECTOR; kk++) 
            {
                dotProd = boundary_x[kk] * x + boundary_y[kk] * y;
                if (dotProd > max) 
                {
                    max  = dotProd;
                    maxi = kk;
                }
                else 
                {
                    if (-dotProd > max) 
                    {
                        max  = -dotProd;
                        maxi = kk + NUM_SECTOR;
                    }
                }
            }
            alfa[j * width * 2 + i * 2    ] = maxi % NUM_SECTOR;
            
            alfa[j * width * 2 + i * 2 + 1] = maxi;  
        }/*for(i = 0; i < width; i++)*/
    }/*for(j = 0; j < height; j++)*/
    tmEnd = std::chrono::high_resolution_clock::now();
    chrono::duration<double> diffTime  = tmEnd   - tmStart;
    std::cout<< "  >getFeatures part1 takes:" << diffTime.count()*1000 <<"(ms)"<<std::endl;

  std::cout<< "  >sizeY :" << sizeY <<" sizeX:"<<sizeX<<std::endl;
  std::cout<< " dump alfa:"<<sizeX<<std::endl;
 if(bCapture){ 
     FILE *fpRconv = fopen("./alfa.data", "w+b");
    if(NULL!= fpRconv)
    {
       fwrite(alfa, sizeof(int), height*width*2, fpRconv);
       fclose(fpRconv);
    }
}

    for(i = 0; i < sizeY; i++)
    {
      for(j = 0; j < sizeX; j++)
      {
        for(ii = 0; ii < k; ii++)
        {
          for(jj = 0; jj < k; jj++)
          {
            if ((i * k + ii > 0) && 
                (i * k + ii < height - 1) && 
                (j * k + jj > 0) && 
                (j * k + jj < width  - 1))
            {
              d = (k * i + ii) * width + (j * k + jj);
              (*map)->map[ i * stringSize + j * (*map)->numFeatures + alfa[d * 2    ]] += 
                  r[d] * w[ii * 2] * w[jj * 2];
              (*map)->map[ i * stringSize + j * (*map)->numFeatures + alfa[d * 2 + 1] + NUM_SECTOR] += 
                  r[d] * w[ii * 2] * w[jj * 2];
              if ((i + nearest[ii] >= 0) && 
                  (i + nearest[ii] <= sizeY - 1))
              {
                (*map)->map[(i + nearest[ii]) * stringSize + j * (*map)->numFeatures + alfa[d * 2    ]             ] += 
                  r[d] * w[ii * 2 + 1] * w[jj * 2 ];
                (*map)->map[(i + nearest[ii]) * stringSize + j * (*map)->numFeatures + alfa[d * 2 + 1] + NUM_SECTOR] += 
                  r[d] * w[ii * 2 + 1] * w[jj * 2 ];
              }
              if ((j + nearest[jj] >= 0) && 
                  (j + nearest[jj] <= sizeX - 1))
              {
                (*map)->map[i * stringSize + (j + nearest[jj]) * (*map)->numFeatures + alfa[d * 2    ]             ] += 
                  r[d] * w[ii * 2] * w[jj * 2 + 1];
                (*map)->map[i * stringSize + (j + nearest[jj]) * (*map)->numFeatures + alfa[d * 2 + 1] + NUM_SECTOR] += 
                  r[d] * w[ii * 2] * w[jj * 2 + 1];
              }
              if ((i + nearest[ii] >= 0) && 
                  (i + nearest[ii] <= sizeY - 1) && 
                  (j + nearest[jj] >= 0) && 
                  (j + nearest[jj] <= sizeX - 1))
              {
                (*map)->map[(i + nearest[ii]) * stringSize + (j + nearest[jj]) * (*map)->numFeatures + alfa[d * 2    ]             ] += 
                  r[d] * w[ii * 2 + 1] * w[jj * 2 + 1];
                (*map)->map[(i + nearest[ii]) * stringSize + (j + nearest[jj]) * (*map)->numFeatures + alfa[d * 2 + 1] + NUM_SECTOR] += 
                  r[d] * w[ii * 2 + 1] * w[jj * 2 + 1];
              }
            }
          }/*for(jj = 0; jj < k; jj++)*/
        }/*for(ii = 0; ii < k; ii++)*/
      }/*for(j = 1; j < sizeX - 1; j++)*/
    }/*for(i = 1; i < sizeY - 1; i++)*/

 if(bCapture){ 
    FILE *fpRconv = fopen("./map.data", "w+b");
    if(NULL!= fpRconv)
    {
       fwrite( (*map)->map, sizeof(float), sizeX*sizeY*27, fpRconv);
       fclose(fpRconv);
    }
}

   // std::cout<<" allocFeatureMapObject-> (*map)->sizeX:"<<(*map)->sizeX<<" (*map)->sizeY:"<<(*map)->sizeY<<" (*map)->numFeatures:"<<(*map)->numFeatures<<std::endl;

    cvReleaseImage(&dx);
    cvReleaseImage(&dy);


    free(w);
    free(nearest);
    
    free(r);
    free(alfa);

    return LATENT_SVM_OK;
}

/*
// Feature map Normalization and Truncation 
//
// API
// int normalizeAndTruncate(featureMap *map, const float alfa);
// INPUT
// map               - feature map
// alfa              - truncation threshold
// OUTPUT
// map               - truncated and normalized feature map
// RESULT
// Error status
*/
int normalizeAndTruncate_orig(CvLSVMFeatureMapCaskade *map, const float alfa)
{
    int i,j, ii;
    int sizeX, sizeY, p, pos, pp, xp, pos1, pos2;
    float * partOfNorm; // norm of C(i, j)
    float * newData;
    float   valOfNorm;

    sizeX     = map->sizeX;
    sizeY     = map->sizeY;
    partOfNorm = (float *)malloc (sizeof(float) * (sizeX * sizeY));
    if (partOfNorm == NULL)
        return -1;
    memset(partOfNorm, 0, sizeof(float) * (sizeX * sizeY));

    p  = NUM_SECTOR;
    xp = NUM_SECTOR * 3;
    pp = NUM_SECTOR * 12;

    for(i = 0; i < sizeX * sizeY; i++)
    {
        valOfNorm = 0.0f;
        pos = i * map->numFeatures;
        for(j = 0; j < p; j++)
        {
            valOfNorm += map->map[pos + j] * map->map[pos + j];
        }/*for(j = 0; j < p; j++)*/
        partOfNorm[i] = valOfNorm;
    }/*for(i = 0; i < sizeX * sizeY; i++)*/
    
    sizeX -= 2;
    sizeY -= 2;

    newData = (float *)malloc (sizeof(float) * (sizeX * sizeY * pp));
    if (newData == NULL)
    {
        if (partOfNorm)
            free(partOfNorm);
        return -1;  
    }
    memset(newData, 0 , sizeof(float) * (sizeX * sizeY * pp));
//normalization
    for(i = 1; i <= sizeY; i++)
    {
        for(j = 1; j <= sizeX; j++)
        {
            valOfNorm = sqrtf(
                partOfNorm[(i    )*(sizeX + 2) + (j    )] +
                partOfNorm[(i    )*(sizeX + 2) + (j + 1)] +
                partOfNorm[(i + 1)*(sizeX + 2) + (j    )] +
                partOfNorm[(i + 1)*(sizeX + 2) + (j + 1)]) + FLT_EPSILON;
            pos1 = (i  ) * (sizeX + 2) * xp + (j  ) * xp;
            pos2 = (i-1) * (sizeX    ) * pp + (j-1) * pp;
            for(ii = 0; ii < p; ii++)
            {
                newData[pos2 + ii        ] = map->map[pos1 + ii    ] / valOfNorm;
            }/*for(ii = 0; ii < p; ii++)*/
            for(ii = 0; ii < 2 * p; ii++)
            {
                newData[pos2 + ii + p * 4] = map->map[pos1 + ii + p] / valOfNorm;
            }/*for(ii = 0; ii < 2 * p; ii++)*/
            valOfNorm = sqrtf(
                partOfNorm[(i    )*(sizeX + 2) + (j    )] +
                partOfNorm[(i    )*(sizeX + 2) + (j + 1)] +
                partOfNorm[(i - 1)*(sizeX + 2) + (j    )] +
                partOfNorm[(i - 1)*(sizeX + 2) + (j + 1)]) + FLT_EPSILON;
            for(ii = 0; ii < p; ii++)
            {
                newData[pos2 + ii + p    ] = map->map[pos1 + ii    ] / valOfNorm;
            }/*for(ii = 0; ii < p; ii++)*/
            for(ii = 0; ii < 2 * p; ii++)
            {
                newData[pos2 + ii + p * 6] = map->map[pos1 + ii + p] / valOfNorm;
            }/*for(ii = 0; ii < 2 * p; ii++)*/
            valOfNorm = sqrtf(
                partOfNorm[(i    )*(sizeX + 2) + (j    )] +
                partOfNorm[(i    )*(sizeX + 2) + (j - 1)] +
                partOfNorm[(i + 1)*(sizeX + 2) + (j    )] +
                partOfNorm[(i + 1)*(sizeX + 2) + (j - 1)]) + FLT_EPSILON;
            for(ii = 0; ii < p; ii++)
            {
                newData[pos2 + ii + p * 2] = map->map[pos1 + ii    ] / valOfNorm;
            }/*for(ii = 0; ii < p; ii++)*/
            for(ii = 0; ii < 2 * p; ii++)
            {
                newData[pos2 + ii + p * 8] = map->map[pos1 + ii + p] / valOfNorm;
            }/*for(ii = 0; ii < 2 * p; ii++)*/
            valOfNorm = sqrtf(
                partOfNorm[(i    )*(sizeX + 2) + (j    )] +
                partOfNorm[(i    )*(sizeX + 2) + (j - 1)] +
                partOfNorm[(i - 1)*(sizeX + 2) + (j    )] +
                partOfNorm[(i - 1)*(sizeX + 2) + (j - 1)]) + FLT_EPSILON;
            for(ii = 0; ii < p; ii++)
            {
                newData[pos2 + ii + p * 3 ] = map->map[pos1 + ii    ] / valOfNorm;
            }/*for(ii = 0; ii < p; ii++)*/
            for(ii = 0; ii < 2 * p; ii++)
            {
                newData[pos2 + ii + p * 10] = map->map[pos1 + ii + p] / valOfNorm;
            }/*for(ii = 0; ii < 2 * p; ii++)*/
        }/*for(j = 1; j <= sizeX; j++)*/
    }/*for(i = 1; i <= sizeY; i++)*/
//truncation
    for(i = 0; i < sizeX * sizeY * pp; i++)
    {
        if(newData [i] > alfa) newData [i] = alfa;
    }/*for(i = 0; i < sizeX * sizeY * pp; i++)*/
//swop data

    map->numFeatures  = pp;
    map->sizeX = sizeX;
    map->sizeY = sizeY;

    free (map->map);
    free (partOfNorm);

    map->map = newData;

    return LATENT_SVM_OK;
}
/*
// Feature map reduction
// In each cell we reduce dimension of the feature vector
// according to original paper special procedure
//
// API
// int PCAFeatureMaps(featureMap *map)
// INPUT
// map               - feature map
// OUTPUT
// map               - feature map
// RESULT
// Error status
*/
int PCAFeatureMaps(CvLSVMFeatureMapCaskade *map)
{ 
    int i,j, ii, jj, k;
    int sizeX, sizeY, p,  pp, xp, yp, pos1, pos2;
    float * newData;
    float val;
    float nx, ny;
    
    sizeX = map->sizeX;
    sizeY = map->sizeY;
    p     = map->numFeatures;
    pp    = NUM_SECTOR * 3 + 4;
    yp    = 4;
    xp    = NUM_SECTOR;

    nx    = 1.0f / sqrtf((float)(xp * 2));
    ny    = 1.0f / sqrtf((float)(yp    ));

    newData = (float *)malloc (sizeof(float) * (sizeX * sizeY * pp));
    if (newData == NULL)
        return -1;

    std::chrono::high_resolution_clock::time_point tmStart;
    std::chrono::high_resolution_clock::time_point tmEnd;
    tmStart = std::chrono::high_resolution_clock::now();
    for(i = 0; i < sizeY; i++)
    {
        for(j = 0; j < sizeX; j++)
        {
            pos1 = ((i)*sizeX + j)*p;
            pos2 = ((i)*sizeX + j)*pp;
            k = 0;
            for(jj = 0; jj < xp * 2; jj++)
            {
                val = 0;
                for(ii = 0; ii < yp; ii++)
                {
                    val += map->map[pos1 + yp * xp + ii * xp * 2 + jj];
                }/*for(ii = 0; ii < yp; ii++)*/
                newData[pos2 + k] = val * ny;
                k++;
            }/*for(jj = 0; jj < xp * 2; jj++)*/
            for(jj = 0; jj < xp; jj++)
            {
                val = 0;
                for(ii = 0; ii < yp; ii++)
                {
                    val += map->map[pos1 + ii * xp + jj];
                }/*for(ii = 0; ii < yp; ii++)*/
                newData[pos2 + k] = val * ny;
                k++;
            }/*for(jj = 0; jj < xp; jj++)*/
            for(ii = 0; ii < yp; ii++)
            {
                val = 0;
                for(jj = 0; jj < 2 * xp; jj++)
                {
                    val += map->map[pos1 + yp * xp + ii * xp * 2 + jj];
                }/*for(jj = 0; jj < xp; jj++)*/
                newData[pos2 + k] = val * nx;
                k++;
            } /*for(ii = 0; ii < yp; ii++)*/           
        }/*for(j = 0; j < sizeX; j++)*/
    }/*for(i = 0; i < sizeY; i++)*/
//swop data
     tmEnd = std::chrono::high_resolution_clock::now();
    chrono::duration<double> diffTime  = tmEnd   - tmStart;
    std::cout<< "  >getFeatures PCA takes:" << diffTime.count()*1000 <<"(ms)"<<std::endl;

    map->numFeatures = pp;

    free (map->map);

    map->map = newData;

    return LATENT_SVM_OK;
}


//modified from "lsvmc_routine.cpp"

int allocFeatureMapObject(CvLSVMFeatureMapCaskade **obj, const int sizeX, 
                          const int sizeY, const int numFeatures)
{
    int i;
    (*obj) = (CvLSVMFeatureMapCaskade *)malloc(sizeof(CvLSVMFeatureMapCaskade));
    if ((*obj) == NULL)
        return -1;
    (*obj)->sizeX       = sizeX;
    (*obj)->sizeY       = sizeY;
    (*obj)->numFeatures = numFeatures;

//memalign(alignment, size);
    (*obj)->map = (float *) memalign(0x1000, sizeof (float) * 
                                  (sizeX * sizeY  * numFeatures));
    if ((*obj)->map == NULL)
        return -1;

    for(i = 0; i < sizeX * sizeY * numFeatures; i++)
    {
        (*obj)->map[i] = 0.0f;
    }
    return LATENT_SVM_OK;
}

int allocFeatureMapObjectExt(CvLSVMFeatureMapCaskade **obj, const int sizeX, 
                          const int sizeY, const int numFeatures, float *buffer)
{
    int i;
    (*obj) = (CvLSVMFeatureMapCaskade *)malloc(sizeof(CvLSVMFeatureMapCaskade));
    if ((*obj) == NULL)
        return -1;
    (*obj)->sizeX       = sizeX;
    (*obj)->sizeY       = sizeY;
    (*obj)->numFeatures = numFeatures;

    //std::cout<<" allocFeatureMapObjectEx->sizeX:"<<sizeX<<" sizeY:"<<sizeY<<" numFeatures:"<<numFeatures<<std::endl;
    (*obj)->map = buffer;

    //std::cout<<" Benson size:"<<sizeof(float) *(sizeX * sizeY  * numFeatures)<<std::endl;
    for(i = 0; i < sizeX * sizeY * numFeatures; i++)
    {
      //  (*obj)->map[i] = 0.0f;
    }
    return LATENT_SVM_OK;
}


int freeFeatureMapObject (CvLSVMFeatureMapCaskade **obj)
{

    if(*obj == NULL) return LATENT_SVM_MEM_NULL;
    free((*obj)->map);
     free(*obj);
     (*obj) = NULL;
   return LATENT_SVM_OK;
}

int freeFeatureMapObjectExt (CvLSVMFeatureMapCaskade **obj)
{
   
   // free(*obj);
   // (*obj) = NULL;
    return LATENT_SVM_OK;
}
