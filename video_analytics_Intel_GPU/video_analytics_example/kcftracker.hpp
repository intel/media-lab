/*

Tracker based on Kernelized Correlation Filter (KCF) [1] and Circulant Structure with Kernels (CSK) [2].
CSK is implemented by using raw gray level features, since it is a single-channel filter.
KCF is implemented by using HOG features (the default), since it extends CSK to multiple channels.

[1] J. F. Henriques, R. Caseiro, P. Martins, J. Batista,
"High-Speed Tracking with Kernelized Correlation Filters", TPAMI 2015.

[2] J. F. Henriques, R. Caseiro, P. Martins, J. Batista,
"Exploiting the Circulant Structure of Tracking-by-detection with Kernels", ECCV 2012.

Authors: Joao Faro, Christian Bailer, Joao F. Henriques
Contacts: joaopfaro@gmail.com, Christian.Bailer@dfki.de, henriques@isr.uc.pt
Institute of Systems and Robotics - University of Coimbra / Department Augmented Vision DFKI


Constructor parameters, all boolean:
    hog: use HOG features (default), otherwise use raw pixels
    fixed_window: fix window size (default), otherwise use ROI size (slower but more accurate)
    multiscale: use multi-scale tracking (default; cannot be used with fixed_window = true)

Default values are set for all properties of the tracker depending on the above choices.
Their values can be customized further before calling init():
    interp_factor: linear interpolation factor for adaptation
    sigma: gaussian kernel bandwidth
    lambda: regularization
    cell_size: HOG cell size
    padding: horizontal area surrounding the target, relative to its size
    output_sigma_factor: bandwidth of gaussian target
    template_size: template size in pixels, 0 to use ROI size
    scale_step: scale step for multi-scale estimation, 1 to disable it
    scale_weight: to downweight detection scores of other scales for added stability

For speed, the value (template_size/cell_size) should be a power of 2 or a product of small prime numbers.

Inputs to init():
   image is the initial frame.
   roi is a cv::Rect with the target positions in the initial frame

Inputs to update():
   image is the current frame.

Outputs of update():
   cv::Rect with target positions for the current frame


By downloading, copying, installing or using the software you agree to this license.
If you do not agree to this license, do not download, install,
copy or use the software.


                          License Agreement
               For Open Source Computer Vision Library
                       (3-clause BSD License)

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

  * Neither the names of the copyright holders nor the names of the contributors
    may be used to endorse or promote products derived from this software
    without specific prior written permission.

This software is provided by the copyright holders and contributors "as is" and
any express or implied warranties, including, but not limited to, the implied
warranties of merchantability and fitness for a particular purpose are disclaimed.
In no event shall copyright holders or contributors be liable for any direct,
indirect, incidental, special, exemplary, or consequential damages
(including, but not limited to, procurement of substitute goods or services;
loss of use, data, or profits; or business interruption) however caused
and on any theory of liability, whether in contract, strict liability,
or tort (including negligence or otherwise) arising in any way out of
the use of this software, even if advised of the possibility of such damage.
 */

#pragma once

#include "tracker.h"
#ifndef __INCLUDE_CMRT_H
#define __INCLUDE_CMRT_H
// The only CM runtime header file that you need is cm_rt.h.
// It includes all of the CM runtime.
#include "cm_rt.h"

// Includes bitmap_helpers.h for bitmap file open/save/compare operations.
#include "bitmap_helpers.h"

#endif
#include "SetupSurface.h"
#include "fhog.hpp"


#ifndef _OPENCV_KCFTRACKER_HPP_
#define _OPENCV_KCFTRACKER_HPP_
#endif

#define MAX_FILELEN 1024
struct sPerfProfileParam
{
	char cSrcFile0[MAX_FILELEN];
	char cSrcFile1[MAX_FILELEN];
	char cParamFile[MAX_FILELEN];
	char cOutFileH0[MAX_FILELEN];
	char cOutFileV0[MAX_FILELEN];
	char cOutFileH1[MAX_FILELEN];
	char cOutFileV1[MAX_FILELEN];
	char cOutFileMulSpec[MAX_FILELEN];
	char cOutDFTInvH[MAX_FILELEN];
	char cOutDFTInvV[MAX_FILELEN];
	char cOutImageSum[MAX_FILELEN];
	char cOutSrcImageSum[MAX_FILELEN];
	char cOutFinal[MAX_FILELEN];

	unsigned short usInWidth;
	unsigned short usInHeight;
	unsigned short ucInFormat;

	unsigned short usOutWidth;
	unsigned short usOutHeight;
	unsigned short usOutFormat;

	unsigned short usSliceNum;
	unsigned short usSubSliceNum;
	unsigned short usEUNum;

	bool           bPrimitive;
	unsigned short usLoops;

    unsigned short usWalkerPattern;

	unsigned short usPitchX;
	unsigned short usPitchY;
	unsigned short usPitchZ;

	float          fScale;
	unsigned char  ucChannel;

	bool           bInv;
};


typedef struct {
    char   *token;
    void   *place;
    int     type;
    int     size;
} InputParamMap;


class KCFTracker : public Tracker
{
public:
    // Constructor
    KCFTracker(bool hog = true, bool fixed_window = true, bool multiscale = true, bool lab = true);

    // Initialize tracker 
    virtual void init(const cv::Rect &roi, unsigned int vaSurfaceID, int width, int height);
    
    // Update position based on the new frame
    virtual cv::Rect update(unsigned int vaSurfaceID, int width, int height);

    float interp_factor; // linear interpolation factor for adaptation
    float sigma; // gaussian kernel bandwidth
    float lambda; // regularization
    int cell_size; // HOG cell size
    int cell_sizeQ; // cell size^2, to avoid repeated operations
    float padding; // extra area surrounding the target
    float output_sigma_factor; // bandwidth of gaussian target
    int template_size; // template size
   // float scale_step; // scale step for multi-scale estimation
   // float scale_weight;  // to downweight detection scores of other scales for added stability

   //Global resource for MDF
   static CmDevice* pCmDev;

protected:
    // Detect object in the current frame.
    cv::Point2f detect(cv::Mat z, cv::Mat x, float &peak_value);

    // train tracker with a single image
    void train(cv::Mat x, float train_interp_factor);

    // Evaluates a Gaussian kernel with bandwidth SIGMA for all relative shifts between input images X and Y, which must both be MxN. They must    also be periodic (ie., pre-processed with a cosine window).
    cv::Mat gaussianCorrelation(cv::Mat x1, cv::Mat x2);
    cv::Mat gaussianCorrelation_gpu(cv::Mat x1, cv::Mat x2);

    // Create Gaussian Peak. Function called only in the first frame.
    cv::Mat createGaussianPeak(int sizey, int sizex);

    // Obtain sub-window from image, with replication-padding and extract features
    cv::Mat getFeatures(unsigned int vaSurfaceID, bool inithann, float scale_adjust = 1.0f, int rawWidth=1920, int rawHeight=1080);
    void getFeatures2(unsigned int vaSurfaceID, bool inithann, float scale_adjust, int rawWidth, int rawHeight);
    // Initialize Hanning window. Function called only in the first frame.
    void createHanningMats();

    // Calculate sub-pixel peak for one dimension
    float subPixelPeak(float left, float center, float right);

    // Initialize Intel GPU accelerator
    int initMDF();

    int getFeatureMaps_gpu(unsigned int vaSurfaceID,  cv::Rect extracted_roi, int rawWidth, int rawHeight, int tempWidth, int tempHeight, const int k, CvLSVMFeatureMapCaskade **map);
    int refreshKernels(int nRawWidth, int nRawheight,cv::Rect extracted_roi, int nWidth, int nHeight);
    int gpu_gaussianCorrelation(unsigned char* InputBuffer0, unsigned char *InputBuffer1, unsigned short usWidth, unsigned short usHeight, unsigned char uChannel, 
                                       unsigned short usPitchX, unsigned short usPitchY, unsigned short usPitchZ, unsigned char *Outputbuffer);
    int Call_SrcImageSum(CmDevice* pCmDev, CmProgram* program, CmSurface2D*  pInputSurf0, CmSurface2D*  pInputSurf1, CmBufferUP* pOutputBufUp, sPerfProfileParam sInParamConfig, float *fExetime);
    int Call_ImageSum(CmDevice* pCmDev, CmProgram* program, CmKernel* kernel, CmBuffer*  pInputSurf0, CmBuffer*  pOutputSurf, sPerfProfileParam sInParamConfig, float *fExetime, CmThreadSpace** pTS);
    int Call_MulSpec(CmDevice* pCmDev, CmProgram* program, CmKernel* kernel, CmBuffer*  pInputSurf0, CmBuffer*  pInputSurf1, CmSurface2D*  pOutputSurf, sPerfProfileParam sInParamConfig, float *fExetime, CmThreadSpace** pTS);
    int Call_FianlExpImageSum(CmDevice* pCmDev, CmProgram* program, CmBuffer*  pInputSurf0, unsigned char *InputBuffer1, unsigned char *OutputBuffer, sPerfProfileParam sInParamConfig, float *fExetime);
    int DFTFactorize( int n, int* factors );
    void DFTInit( int n0, int nf, int* factors, int* itab, int elem_size, float* _wave, int inv_itab );
    int Call_DFT_FWD_H(CmDevice* pCmDev, CmProgram* program, CmKernel* kernel, CmSurface2D*  pInputSurf, CmBufferUP *pParamSurfHUP, CmBuffer*  pOutputSurf, sPerfProfileParam sInParamConfig, int nf, float *fExetime, CmThreadSpace** pTS);
    int Call_DFT_FWD_V(CmDevice* pCmDev, CmProgram* program, CmKernel* kernel, CmBuffer*  pInputSurf, CmBufferUP *ParamBufferUP, CmBuffer* pOutputSurf, sPerfProfileParam sInParamConfig, int nf, float *fExetime, CmThreadSpace** pTS);

    cv::Mat _alphaf;
    cv::Mat _prob;
    cv::Mat _tmpl;
    cv::Mat _num;
    cv::Mat _den;
    cv::Mat _labCentroids;

private:
    int size_patch[3];
    cv::Mat hann;
    cv::Size _tmpl_sz;
    float _scale;
    int _gaussian_size;
    bool _hogfeatures;
    bool _labfeatures;

    CmKernel* featureKernel;
    CmKernel* processKernel;
    CmKernel* normalizePCAKernel;
    CmProgram* programHogFeature;
    CmProgram* programGaussianCorrection;

    CmQueue* pCmQueueGetFeatures; 
    CmQueue* pCmQueueGaussianCorrection;
    CmSampler8x8   *pSampler8x8;
    std::map<long long, CmSurface2D*>   m_pCmSurfaceNV12;
    CvLSVMFeatureMapCaskade *SVMFeaturemap = NULL;
    std::map<long long, SamplerIndex*> m_mInputSamplerIndexPool;
    std::map<long long, SurfaceIndex*> m_mISurfaceIndexPool;

    CmSurface2D* pCmSurface2D_out;
    CmSurface2D* pCmSurface2D_normal;
    CmSurface2D* pCmSurface2D_verify;
    //CmSurface2DUP* pCmSurface2D_newFeature;
    CmSurface2D* pCmSurface2D_newFeature;
    int inputWidth;
    int inputHeight;
    int threadswidth;
    int threadsheight;
    bool bsetupKernelOnce;
    void *featuremap;
    cv::Rect previous_roi;

    int templateWidth;
    int templateHeight;
    //
    sPerfProfileParam sInParamConfig;
    void          *pCommonISACode   = NULL;

    CmKernel*  kernel_H_F_0;
    CmKernel*  kernel_V_F_0;
    CmKernel*  kernel_H_F_1;
    CmKernel*  kernel_V_F_1;
    CmKernel*  kernel_H_I;
    CmKernel*  kernel_V_I;
    CmKernel*  kernel_MulSpec;
    CmKernel*  kernel_ImageSum;
    CmKernel*  kernelFinalExp;
    CmKernel*  kernelSrcImageSum;

    CmBuffer*  pInputSumSurf;
    CmBuffer*  pOutputFinalSurf;
    CmSurface2D*  pInputSurf0 ;
    CmSurface2D*  pInputSurf1 ;
    CmBuffer*  pOutputSurf_H_F_0;
    CmBuffer*  pOutputSurf_V_F_0;
    CmBuffer*  pOutputSurf_H_F_1;
    CmBuffer*  pOutputSurf_V_F_1;
    CmSurface2D*  pMulSpectrumSurf;
    CmBuffer*   pOutputSurf_H_V_1;
    CmBuffer*   pOutputSurf_V_V_1;
    CmBuffer*   pOutputInvSumSurf;

    CmBuffer*   pParamSurfH;
    CmBufferUP* pParamSurfHUP ;

    CmBuffer*   pParamSurfV;
    CmBufferUP* pParamBufferV ;
    CmBuffer*   pOutputImageSurf;

    unsigned char *pParamBufferULTV;
    unsigned char *pParamBufferULTH;
    unsigned char *SrcImageSumBuffer;
    CmBufferUP *pSrcImageSumBufferUp;
    unsigned short gPitchX;
    unsigned short gPitchY;
    unsigned short gPitchZ;

    unsigned short gWidth;
    unsigned short gHeight;

    int   nfx ;
    int   nfy ;
    CmTask*    pKernelArray;
    CmTask *pKernelArrayGetFeatures;        



};
