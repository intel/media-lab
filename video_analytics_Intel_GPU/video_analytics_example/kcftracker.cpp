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
    padding: area surrounding the target, relative to its size
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

#ifndef _KCFTRACKER_HEADERS
#include "kcftracker.hpp"
#include "ffttools.hpp"
#include "recttools.hpp"
#include "fhog.hpp"
#include "labdata.hpp"
#endif
#include <chrono>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>
#include "fhog.hpp"
#include "opencv2/imgproc/imgproc_c.h"
#include "intelscalartbl.h"
using namespace std;


#define NUM_SECTOR 9

// Constructor
KCFTracker::KCFTracker(bool hog, bool fixed_window, bool multiscale, bool lab):
    interp_factor(0.0),
    sigma(0.0),
    lambda(0.0),
    cell_size(0),
    cell_sizeQ(0),
    padding(0.0),
    output_sigma_factor(0.0),
    template_size(0),
    _scale(0.0),
    _gaussian_size(0),
    _hogfeatures(false),
    _labfeatures(false),

    featureKernel(NULL),
    processKernel(NULL),
    normalizePCAKernel(NULL),
    programHogFeature(NULL),
    programGaussianCorrection(NULL),

    pCmQueueGetFeatures(NULL),
    pCmQueueGaussianCorrection(NULL),
    pSampler8x8(NULL),

    pCmSurface2D_out(NULL),
    pCmSurface2D_normal(NULL),
    pCmSurface2D_verify(NULL),
    pCmSurface2D_newFeature(NULL),
    inputWidth(0),
    inputHeight(0),
    threadswidth(0),
    threadsheight(0),
    bsetupKernelOnce(false),
    featuremap(NULL),

    templateWidth(0),
    templateHeight(0),

    kernel_H_F_0(NULL),
    kernel_V_F_0(NULL),
    kernel_H_F_1(NULL),
    kernel_V_F_1(NULL),
    kernel_H_I(NULL),
    kernel_V_I(NULL),
    kernel_MulSpec(NULL),
    kernel_ImageSum(NULL),
    kernelFinalExp(NULL),
    kernelSrcImageSum(NULL),

    pInputSumSurf(NULL),
    pOutputFinalSurf(NULL),
    pInputSurf0(NULL),
    pInputSurf1(NULL),
    pOutputSurf_H_F_0(NULL),
    pOutputSurf_V_F_0(NULL),
    pOutputSurf_H_F_1(NULL),
    pOutputSurf_V_F_1(NULL),
    pMulSpectrumSurf(NULL),
    pOutputSurf_H_V_1(NULL),
    pOutputSurf_V_V_1(NULL),
    pOutputInvSumSurf(NULL),

    pParamSurfH(NULL),
    pParamSurfHUP(NULL),

    pParamSurfV(NULL),
    pParamBufferV (NULL),
    pOutputImageSurf(NULL),

    pParamBufferULTV(NULL),
    pParamBufferULTH(NULL),
    SrcImageSumBuffer(NULL),
    pSrcImageSumBufferUp(NULL),
    gPitchX(0),
    gPitchY(0),
    gPitchZ(0),

    gWidth(0),
    gHeight(0),

    nfx(0),
    nfy(0),
    pKernelArray(NULL),
    pKernelArrayGetFeatures(NULL)
{
    // Parameters equal in all cases
    lambda = 0.0001;
    padding = 2.5; 
    //output_sigma_factor = 0.1;
    output_sigma_factor = 0.125;

    // focus on HOG only
    // VOT
    interp_factor = 0.012;
    sigma = 0.6; 
    // TPAMI
    //interp_factor = 0.02;
    //sigma = 0.5; 
    cell_size = 4;
    _hogfeatures = true;

    // use multiple scale only
    template_size = 96;
   // scale_step = 1.20;//1.05;
   // scale_weight = 0.95;

    memset(&sInParamConfig, 0, sizeof(sInParamConfig));

    //Initialize MDF kernel
    initMDF();
    
}

// Initialize tracker 
void KCFTracker::init(const cv::Rect &roi, unsigned int vaSurfaceID, int width, int height)
{
    _roi = roi;
    assert(roi.width >= 0 && roi.height >= 0);

    _tmpl = getFeatures(vaSurfaceID, 1, 1.0f, width, height);
    _prob = createGaussianPeak(size_patch[0], size_patch[1]);
    _alphaf = cv::Mat(size_patch[0], size_patch[1], CV_32FC2, float(0));
    train(_tmpl, 1.0); // train with initial frame
 }
// Update position based on the new frame
cv::Rect KCFTracker::update(unsigned int vaSurfaceID,  int width, int  height)
{
    std::chrono::high_resolution_clock::time_point tmStart;
    std::chrono::high_resolution_clock::time_point tmEnd;
    chrono::duration<double> diffTime;

    tmStart = std::chrono::high_resolution_clock::now();

    if (_roi.x + _roi.width <= 0) _roi.x = -_roi.width + 1;
    if (_roi.y + _roi.height <= 0) _roi.y = -_roi.height + 1;
    if (_roi.x >= width - 1) _roi.x = width - 2;
    if (_roi.y >= height - 1) _roi.y = height - 2;

    float cx = _roi.x + _roi.width / 2.0f;
    float cy = _roi.y + _roi.height / 2.0f;


    float peak_value;
    //Test at a original size
    cv::Point2f res = detect(_tmpl, getFeatures(vaSurfaceID, 0, 1.0f, width, height), peak_value);

    /*if (scale_step != 1) {
        // Test at a smaller _scale
        float new_peak_value;
        cv::Point2f new_res = detect(_tmpl, getFeatures(vaSurfaceID, 0, 1.0f / scale_step, width, height), new_peak_value);

        if (scale_weight * new_peak_value > peak_value) {
            res = new_res;
            peak_value = new_peak_value;
            _scale /= scale_step;
            _roi.width /= scale_step;
            _roi.height /= scale_step;
        }

        // Test at a bigger _scale
        new_res = detect(_tmpl, getFeatures(vaSurfaceID, 0, scale_step, width, height), new_peak_value);

        if (scale_weight * new_peak_value > peak_value) {
            res = new_res;
            peak_value = new_peak_value;
            _scale *= scale_step;
            _roi.width *= scale_step;
            _roi.height *= scale_step;
        }
    }
    */

    // Adjust by cell size and _scale
    _roi.x = cx - _roi.width / 2.0f + ((float) res.x * cell_size * _scale);
    _roi.y = cy - _roi.height / 2.0f + ((float) res.y * cell_size * _scale);

    if (_roi.x >= width - 1)  _roi.x = width - 1;
    if (_roi.y >= height - 1)  _roi.y = height - 1;
    if (_roi.x + _roi.width <= 0)  _roi.x = -_roi.width + 2;
    if (_roi.y + _roi.height <= 0) _roi.y = -_roi.height + 2;

    assert(_roi.width >= 0 && _roi.height >= 0);
    cv::Mat x = getFeatures(vaSurfaceID, 0, 1.0f, width, height);

    tmEnd = std::chrono::high_resolution_clock::now();
    diffTime  = tmEnd   - tmStart;
   // std::cout<< "  Update 1st part  takes: :" << diffTime.count()*1000 <<"(ms)"<<std::endl;
        
    tmStart = std::chrono::high_resolution_clock::now();

    train(x, interp_factor);
    tmEnd = std::chrono::high_resolution_clock::now();
        diffTime  = tmEnd   - tmStart;
    //    std::cout<< "  Update 2nd part  takes: :" << diffTime.count()*1000 <<"(ms)"<<std::endl;


        

    return _roi;
}


// Detect object in the current frame.
cv::Point2f KCFTracker::detect(cv::Mat z, cv::Mat x, float &peak_value)
{
    using namespace FFTTools;
    std::chrono::high_resolution_clock::time_point tmStart;
    std::chrono::high_resolution_clock::time_point tmEnd;
    tmStart = std::chrono::high_resolution_clock::now();

    cv::Mat k = gaussianCorrelation_gpu(x, z);
    cv::Mat res = (real(fftd(complexMultiplication(_alphaf, fftd(k)), true)));

    //minMaxLoc only accepts doubles for the peak, and integer points for the coordinates
    cv::Point2i pi;
    double pv;
    cv::minMaxLoc(res, NULL, &pv, NULL, &pi);
    peak_value = (float) pv;

    //subpixel peak estimation, coordinates will be non-integer
    cv::Point2f p((float)pi.x, (float)pi.y);

    if (pi.x > 0 && pi.x < res.cols-1) {
        p.x += subPixelPeak(res.at<float>(pi.y, pi.x-1), peak_value, res.at<float>(pi.y, pi.x+1));
    }

    if (pi.y > 0 && pi.y < res.rows-1) {
        p.y += subPixelPeak(res.at<float>(pi.y-1, pi.x), peak_value, res.at<float>(pi.y+1, pi.x));
    }

    p.x -= (res.cols) / 2;
    p.y -= (res.rows) / 2;
    tmEnd = std::chrono::high_resolution_clock::now();
    chrono::duration<double> diffTime  = tmEnd   - tmStart;
    //std::cout<< "  >detect takes:" << diffTime.count()*1000 <<"(ms)"<<std::endl;

    return p;
}

// train tracker with a single image
void KCFTracker::train(cv::Mat x, float train_interp_factor)
{
    using namespace FFTTools;
    std::chrono::high_resolution_clock::time_point tmStart;
    std::chrono::high_resolution_clock::time_point tmEnd;
    tmStart = std::chrono::high_resolution_clock::now();

    cv::Mat k = gaussianCorrelation_gpu(x, x);
    cv::Mat alphaf = complexDivision(_prob, (fftd(k) + lambda));
    
    _tmpl = (1 - train_interp_factor) * _tmpl + (train_interp_factor) * x;
    _alphaf = (1 - train_interp_factor) * _alphaf + (train_interp_factor) * alphaf;


    /*cv::Mat kf = fftd(gaussianCorrelation(x, x));
    cv::Mat num = complexMultiplication(kf, _prob);
    cv::Mat den = complexMultiplication(kf, kf + lambda);
    
    _tmpl = (1 - train_interp_factor) * _tmpl + (train_interp_factor) * x;
    _num = (1 - train_interp_factor) * _num + (train_interp_factor) * num;
    _den = (1 - train_interp_factor) * _den + (train_interp_factor) * den;

    _alphaf = complexDivision(_num, _den);*/

}

// Evaluates a Gaussian kernel with bandwidth SIGMA for all relative shifts between input images X and Y, which must both be MxN. They must    also be periodic (ie., pre-processed with a cosine window).
cv::Mat KCFTracker::gaussianCorrelation(cv::Mat x1, cv::Mat x2)
{
    using namespace FFTTools;

    std::chrono::high_resolution_clock::time_point tmStart;
    std::chrono::high_resolution_clock::time_point tmEnd;
    tmStart = std::chrono::high_resolution_clock::now();
    //std::cout<<"size_patch[0]:"<<size_patch[0]<<" size_patch[1]:"<<size_patch[1]<< " size_patch[2]"<<size_patch[2]<<std::endl;
    cv::Mat c = cv::Mat( cv::Size(size_patch[1], size_patch[0]), CV_32F, cv::Scalar(0) );
    // HOG features
    if (1) {
        cv::Mat caux;
        cv::Mat x1aux;
        cv::Mat x2aux;
        //std::cout<<"row:"<<x1.rows<<" cols:"<<x1.cols<<std::endl;
        for (int i = 0; i < size_patch[2]; i++) {
            x1aux = x1.row(i);   // Procedure do deal with cv::Mat multichannel bug
            x1aux = x1aux.reshape(1, size_patch[0]);
            x2aux = x2.row(i).reshape(1, size_patch[0]);
            cv::mulSpectrums(fftd(x1aux), fftd(x2aux), caux, 0, true); 
            caux = fftd(caux, true);
            rearrange(caux);
            caux.convertTo(caux,CV_32F);
            c = c + real(caux);
        }
    }

    cv::Mat d; 
    cv::max(( (cv::sum(x1.mul(x1))[0] + cv::sum(x2.mul(x2))[0])- 2. * c) / (size_patch[0]*size_patch[1]*size_patch[2]) , 0, d);

    cv::Mat k;
    cv::exp((-d / (sigma * sigma)), k);


    tmEnd = std::chrono::high_resolution_clock::now();
    chrono::duration<double> diffTime  = tmEnd   - tmStart;
    std::cout<< "  >gaussianCorrelation takes:" << diffTime.count()*1000 <<"(ms)"<<std::endl;
    return c;
}
extern int gpu_gaussianCorrelation(unsigned char* InputBuffer0, unsigned char *InputBuffer1, unsigned short usWidth, unsigned short usHeight, unsigned char uChannel, unsigned short usPitchX, unsigned short usPitchY, unsigned short usPitchZ, unsigned char *Outputbuffer);

cv::Mat KCFTracker::gaussianCorrelation_gpu(cv::Mat x1, cv::Mat x2)
{
     std::chrono::high_resolution_clock::time_point tmStart;
     std::chrono::high_resolution_clock::time_point tmEnd;
     chrono::duration<double> diffTime;
     

    cv::Mat k = cv::Mat( cv::Size(size_patch[1], size_patch[0]), CV_32F, cv::Scalar(0) );

	unsigned char * pX1  = x1.data;
	unsigned char * pX2  = x2.data;
	unsigned char * pDst = k.data;

    	tmStart = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < 1; i++)
	{
	    gpu_gaussianCorrelation(pX1, pX2, x1.cols, x1.rows, 1, size_patch[1], size_patch[0], size_patch[2], pDst);
	}

	//printf("tt = %d\n", tt);
    tmEnd = std::chrono::high_resolution_clock::now();
    diffTime  = tmEnd   - tmStart;
    //std::cout<< "  >gaussianCorrelation_gpu takes:" << diffTime.count()*1000 <<"(ms)"<<std::endl;

    return k;
}

// Create Gaussian Peak. Function called only in the first frame.
cv::Mat KCFTracker::createGaussianPeak(int sizey, int sizex)
{
    cv::Mat_<float> res(sizey, sizex);

    int syh = (sizey) / 2;
    int sxh = (sizex) / 2;

    float output_sigma = std::sqrt((float) sizex * sizey) / padding * output_sigma_factor;
    float mult = -0.5 / (output_sigma * output_sigma);

    for (int i = 0; i < sizey; i++)
        for (int j = 0; j < sizex; j++)
        {
            int ih = i - syh;
            int jh = j - sxh;
            res(i, j) = std::exp(mult * (float) (ih * ih + jh * jh));
        }
    return FFTTools::fftd(res);
}


// Obtain sub-window from image, with replication-padding and extract features
cv::Mat KCFTracker::getFeatures(unsigned int vaSurfaceID, bool inithann, float scale_adjust, int rawWidth, int rawHeight)
{

    std::chrono::high_resolution_clock::time_point tmStart;
    std::chrono::high_resolution_clock::time_point tmEnd;
    chrono::duration<double> diffTime;
    tmStart = std::chrono::high_resolution_clock::now();

    cv::Rect extracted_roi;

    float cx = _roi.x + _roi.width / 2;
    float cy = _roi.y + _roi.height / 2;

    if (inithann) {
        int padded_w = _roi.width * padding;
        int padded_h = _roi.height * padding;
        
        if (template_size > 1) {  // Fit largest dimension to the given template size
            if (padded_w >= padded_h)  //fit to width
                _scale = padded_w / (float) template_size;
            else
                _scale = padded_h / (float) template_size;

            _tmpl_sz.width = padded_w / _scale;
            _tmpl_sz.height = padded_h / _scale;
        }
        else {  //No template size given, use ROI size
            _tmpl_sz.width = padded_w;
            _tmpl_sz.height = padded_h;
            _scale = 1;
            // original code from paper:
        }

     
            // Round to cell size and also make it even
            _tmpl_sz.width = ( ( (int)(_tmpl_sz.width / (2 * cell_size)) ) * 2 * cell_size ) + cell_size*2;
            _tmpl_sz.height = ( ( (int)(_tmpl_sz.height / (2 * cell_size)) ) * 2 * cell_size ) + cell_size*2;
       
    }

    extracted_roi.width = scale_adjust * _scale * _tmpl_sz.width;
    extracted_roi.height = scale_adjust * _scale * _tmpl_sz.height;

    // center roi with new size
    extracted_roi.x = cx - extracted_roi.width / 2;
    extracted_roi.y = cy - extracted_roi.height / 2;
    cv::Mat FeaturesMap; 

    if (1) {
      

        getFeatureMaps_gpu(vaSurfaceID, extracted_roi, rawWidth, rawHeight, _tmpl_sz.width, _tmpl_sz.height,cell_size, &SVMFeaturemap);

        if (SVMFeaturemap)
        { 
            size_patch[0] = SVMFeaturemap->sizeY;
            size_patch[1] = SVMFeaturemap->sizeX;
            size_patch[2] = SVMFeaturemap->numFeatures;

            FeaturesMap = cv::Mat(cv::Size(SVMFeaturemap->numFeatures,SVMFeaturemap->sizeX*SVMFeaturemap->sizeY), CV_32F, SVMFeaturemap->map);  // Procedure do deal with cv::Mat multichannel bug
            FeaturesMap = FeaturesMap.t();
        }
       // freeFeatureMapObject(&map);
    }
 
    if (inithann) {
        createHanningMats();
    }
   
    FeaturesMap = hann.mul(FeaturesMap);
    tmEnd = std::chrono::high_resolution_clock::now();
    diffTime  = tmEnd   - tmStart;
   // std::cout<< "  >GetFeatures takes:" << diffTime.count()*1000 <<"(ms)"<<std::endl;

    return FeaturesMap;
}
    
// Initialize Hanning window. Function called only in the first frame.
void KCFTracker::createHanningMats()
{   
    cv::Mat hann1t = cv::Mat(cv::Size(size_patch[1],1), CV_32F, cv::Scalar(0));
    cv::Mat hann2t = cv::Mat(cv::Size(1,size_patch[0]), CV_32F, cv::Scalar(0)); 

    for (int i = 0; i < hann1t.cols; i++)
        hann1t.at<float > (0, i) = 0.5 * (1 - std::cos(2 * 3.14159265358979323846 * i / (hann1t.cols - 1)));
    for (int i = 0; i < hann2t.rows; i++)
        hann2t.at<float > (i, 0) = 0.5 * (1 - std::cos(2 * 3.14159265358979323846 * i / (hann2t.rows - 1)));

    cv::Mat hann2d = hann2t * hann1t;

    cv::Mat hann1d = hann2d.reshape(1,1); // Procedure do deal with cv::Mat multichannel bug
        
    hann = cv::Mat(cv::Size(size_patch[0]*size_patch[1], size_patch[2]), CV_32F, cv::Scalar(0));
    for (int i = 0; i < size_patch[2]; i++) {
            for (int j = 0; j<size_patch[0]*size_patch[1]; j++) {
                hann.at<float>(i,j) = hann1d.at<float>(0,j);
            }
    }

}

// Calculate sub-pixel peak for one dimension
float KCFTracker::subPixelPeak(float left, float center, float right)
{   
    float divisor = 2 * center - right - left;

    if (divisor == 0)
        return 0;
    
    return 0.5 * (right - left) / divisor;
}

extern VADisplay m_va_dpy;
CmDevice* KCFTracker::pCmDev =NULL;

int KCFTracker::initMDF()
{
    int  result;
    UINT version = 0;
    if(NULL == m_va_dpy)
    {
        std::cout<<"KCFGPU: initMDF failed due to m_va_dpy = NULL"<<std::endl;
        return -1;
    }

    if (KCFTracker::pCmDev == NULL)
    {
        result = ::CreateCmDevice(KCFTracker::pCmDev, version, m_va_dpy);
        if (result != CM_SUCCESS ) {
            std::cout<<"CmDevice creation error"<<std::endl;
            return -1;
        }
        if( version < CM_1_0 ){
            std::cout<<" The runtime API version is later than runtime DLL version"<<std::endl;
            return -1;
        }
        std::cout<<"A new MDF device was created success"<<std::endl;
    }

    bsetupKernelOnce = false;
    featuremap =NULL;
    templateWidth = -1;
    templateHeight = -1;
    featureKernel       = NULL;
    processKernel       = NULL;
    normalizePCAKernel  = NULL;
    pCmQueueGetFeatures        = NULL;
    pCmQueueGaussianCorrection = NULL;
    programHogFeature          = NULL;
    programGaussianCorrection  = NULL;
    pSampler8x8         = NULL;
    pCmSurface2D_out    = NULL;
    pCmSurface2D_normal = NULL;
    pCmSurface2D_verify = NULL;
    pCmSurface2D_newFeature = NULL;

    kernel_H_F_0      = NULL;
    kernel_V_F_0      = NULL;
    kernel_H_F_1      = NULL;
    kernel_V_F_1      = NULL;
    kernel_H_I        = NULL;
    kernel_V_I        = NULL;
    kernel_MulSpec    = NULL;
    kernel_ImageSum   = NULL;
    kernelFinalExp    = NULL;
    kernelSrcImageSum = NULL;    
    
    
    pInputSumSurf      = NULL;
    pOutputFinalSurf   = NULL;
    pInputSurf0       = NULL;
    pInputSurf1       = NULL;
    pOutputSurf_H_F_0    = NULL;
    pOutputSurf_V_F_0    = NULL;
    pOutputSurf_H_F_1    = NULL;
    pOutputSurf_V_F_1    = NULL;
    pMulSpectrumSurf  = NULL;
    pOutputSurf_H_V_1    = NULL;
    pOutputSurf_V_V_1    = NULL;
    pOutputInvSumSurf    = NULL;
    
    pParamSurfH  = NULL;
    pParamSurfHUP = NULL ;
    pParamSurfV = NULL;
    pParamBufferV = NULL ;
    pOutputImageSurf = NULL;
    pParamBufferULTV = NULL;
    pParamBufferULTH = NULL;
    SrcImageSumBuffer = NULL;
    pSrcImageSumBufferUp = NULL;
    gPitchX          = 0;
    gPitchY          = 0;
    gPitchZ          = 0;
    gWidth  = 0;
    gHeight = 0;

    
    nfx   =  0;
    nfy   =  0;
    pKernelArray = NULL;
    pKernelArrayGetFeatures = NULL;

    // Open isa file
    // Load kernel into CmProgram obj
    if (programGaussianCorrection == NULL)
    {
        FILE* pISA = fopen("../../kcfGPU/kcf_correlation_genx.isa", "rb");
        if(NULL == pISA){
           std::cout<<"Failed to open kcf_correction_genx.isa"<<std::endl;
           exit(-1);
        }

        // Find the kernel code size
        fseek (pISA, 0, SEEK_END);
        int codeSize = ftell (pISA);
        rewind(pISA);

        if(codeSize == 0) {
            fclose(pISA);
            perror("Kernel code size is 0");
            return -1;
        }

        // Allocate memory to hold isa kernel from kernel file
        if (pCommonISACode == NULL)
        {
            pCommonISACode = (BYTE*) malloc(codeSize);
            if( !pCommonISACode ) {
                fclose(pISA);
                perror("Failed allocate memeory for kernel");
                return -1;
            }

            // Read kernel file
            if (fread(pCommonISACode, 1, codeSize, pISA) != codeSize) {
                fclose(pISA);
                perror("Error reading isa");
                return -1;
            }
        }
        fclose(pISA);

        result = pCmDev->LoadProgram(pCommonISACode, codeSize, programGaussianCorrection); 
        if (result != CM_SUCCESS ) {
            perror("CM LoadProgram error");
            return -1;
        }
        free(pCommonISACode);
        pCommonISACode = NULL;
    }


    if (pKernelArray == NULL)
    {
        result = pCmDev->CreateTask(pKernelArray);
        if (result != CM_SUCCESS ) {
            printf("CmDevice CreateTask error");
            return -1;
        }
    }

    // Create a task queue
    if  (pCmQueueGaussianCorrection == NULL)
    {
        result = pCmDev->CreateQueue( pCmQueueGaussianCorrection );
        if (result != CM_SUCCESS ) {
            perror("CM CreateQueue error");
            return -1;
        }
    }

    std::cout<<"KCFGPU: Init MDF success: "<<std::endl; 

    return 0;
}

int KCFTracker::refreshKernels(int nRawWidth, int nRawHeight, cv::Rect extracted_roi, int nInputWidth, int nInputHeight)
{
    int result;

    int uBlockWidth  = 4;
    int uBlockHeight = 4;
    SurfaceIndex* indexOutVerify     = NULL;
    SurfaceIndex* indexOutInfoInput  = NULL;
    SurfaceIndex* indexOutNormal     = NULL;
    SurfaceIndex* indexOutNewFeature = NULL;


    float boundary_x[NUM_SECTOR + 1];
    float boundary_y[NUM_SECTOR + 1];
    int   nearest[4];
    float w[8];
    float pi = atan(1)*4.0;
    
    int k=4;
    float arg_vector;
    for(int i = 0; i <= NUM_SECTOR; i++)
    {
          arg_vector    = ( (float) i ) * ( (float)(pi) / (float)(NUM_SECTOR) );
          boundary_x[i] = cosf(arg_vector);
          boundary_y[i] = sinf(arg_vector);
    }/*for(i = 0; i <= NUM_SECTOR; i++) */
    
    float  a_x, b_x;
    for(int i = 0; i < k / 2; i++)
    {
      nearest[i] = -1;
    }/*for(i = 0; i < k / 2; i++)*/
    for(int i = k / 2; i < k; i++)
    {
      nearest[i] = 1;
    }/*for(i = k / 2; i < k; i++)*/

    for(int j = 0; j < k / 2; j++)
    {
      b_x = k / 2 + j + 0.5f;
      a_x = k / 2 - j - 0.5f;
      w[j * 2    ] = 1.0f/a_x * ((a_x * b_x) / ( a_x + b_x)); 
      w[j * 2 + 1] = 1.0f/b_x * ((a_x * b_x) / ( a_x + b_x));  
    }/*for(j = 0; j < k / 2; j++)*/
    for(int j = k / 2; j < k; j++)
    {
      a_x = j - k / 2 + 0.5f;
      b_x =-j + k / 2 - 0.5f + k;
      w[j * 2    ] = 1.0f/a_x * ((a_x * b_x) / ( a_x + b_x)); 
      w[j * 2 + 1] = 1.0f/b_x * ((a_x * b_x) / ( a_x + b_x));  
    }/*for(j = k / 2; j < k; j++)*/

    //  int  *alfa = (int   *)malloc( sizeof(int  ) * (nInputWidth * nInputHeight * 2));
    // memset(alfa, 0,  sizeof(int  ) * (nInputWidth * nInputHeight * 2));
    int sizeX = nInputWidth  / 4;
    int sizeY = nInputHeight / 4;
    int px    = 3 * NUM_SECTOR; 
    int p     = px;
 
    // Calculate the offset of left corner(sx, sy)
    float sx = (extracted_roi.x * 1.0)/nRawWidth;
    float sy = (extracted_roi.y * 1.0)/nRawHeight;
    float fDeltaU = 1.0f * extracted_roi.width / ((float)nInputWidth*nRawWidth);
    float fDeltaV = 1.0f * extracted_roi.height / ((float)nInputHeight*nRawHeight);
    

    if( NULL != pSampler8x8)
    {
        pCmDev->DestroySampler8x8(pSampler8x8);
        pSampler8x8 = NULL;

    }
    // Setup AVS Scalar
    result = setupAVSSampler(pCmDev,
                             nInputWidth, // input to the feature extraction
                             nInputHeight,// input to the feature extraction
                             &pSampler8x8);
    if(result != 0 )
    {
        std::cout<<" failed to setup AVS sampler"<<std::endl;
        return -1;
    }
    if( NULL!= pCmSurface2D_verify)
    {
        pCmDev->DestroySurface(pCmSurface2D_verify);
        pCmSurface2D_verify = NULL;
    }
    // output
    surfaceInfoS surfaceOutVerify;
    surfaceOutVerify.fileName      = NULL;
    surfaceOutVerify.width         = (nInputWidth/4)*sizeof(float)*32;
    surfaceOutVerify.height        = (nInputHeight/4)*9;   // we allocate a bigger surface to workload float adding issue
    surfaceOutVerify.surfaceFormat = (char *)("R8");
    surfaceOutVerify.inited = 0;
    pCmSurface2D_verify = setupOutputSurface(pCmDev, surfaceOutVerify);

    
    // output
    if( NULL != pCmSurface2D_out){
        pCmDev->DestroySurface(pCmSurface2D_out);
        pCmSurface2D_out = NULL;
    }
    surfaceInfoS surfaceOutInfoInput;
    surfaceOutInfoInput.fileName      = NULL;
    surfaceOutInfoInput.width         = (nInputWidth/4)*sizeof(float)*27;
    surfaceOutInfoInput.height        = nInputHeight/4;
    surfaceOutInfoInput.surfaceFormat = (char *)("R8");
    surfaceOutInfoInput.inited = 0;
    pCmSurface2D_out = setupOutputSurface(pCmDev, surfaceOutInfoInput);
   
    if( NULL != pCmSurface2D_normal ){
        pCmDev->DestroySurface(pCmSurface2D_normal);
        pCmSurface2D_normal = NULL;

    }
    surfaceInfoS surfaceOutNormal;
    surfaceOutNormal.fileName      = NULL;
    surfaceOutNormal.width         =  (nInputWidth/4)*sizeof(float);
    surfaceOutNormal.height        =  nInputHeight/4;
    surfaceOutNormal.surfaceFormat = (char *)("R8");
    surfaceOutNormal.inited = 0;
    pCmSurface2D_normal = setupOutputSurface(pCmDev, surfaceOutNormal);
   
    if( NULL != pCmSurface2D_newFeature){
        pCmDev->DestroySurface(pCmSurface2D_newFeature);
        pCmSurface2D_newFeature = NULL;

    }
    surfaceInfoS surfaceOutNewFeature;
    surfaceOutNewFeature.fileName      = NULL;
    surfaceOutNewFeature.width         =  (nInputWidth/4-2)*sizeof(float)*31;
    surfaceOutNewFeature.height        =  (nInputHeight/4-2);
    surfaceOutNewFeature.surfaceFormat = (char *)("R8");
    surfaceOutNewFeature.inited = 0;
        //featuremap = (void *)CM_ALIGNED_MALLOC(surfaceOutNewFeature.width * surfaceOutNewFeature.height*10,0x1000);

#if 1

    pCmSurface2D_newFeature = setupOutputSurface(pCmDev, surfaceOutNewFeature);

#else
    pCmSurface2D_newFeature = setupOutputSurfaceExt(pCmDev, surfaceOutNewFeature, indexOutNewFeature,(void *)featuremap);
#endif	

    int kernel_size  = 4;
    int outputStride = (nInputWidth/kernel_size)*27;
    int numofFeature = 27;

    threadswidth  = (unsigned int)(ceil(double(nInputWidth  / 4)));
    threadsheight = (unsigned int)(ceil(double(nInputHeight / 4)));
    featureKernel->SetThreadCount( threadswidth*threadsheight );
    processKernel->SetThreadCount( threadswidth*threadsheight );
    normalizePCAKernel->SetThreadCount( threadswidth*threadsheight );

    pCmSurface2D_newFeature->GetIndex(indexOutNewFeature);
    pCmSurface2D_normal->GetIndex(indexOutNormal);
    pCmSurface2D_out->GetIndex(indexOutInfoInput);
    pCmSurface2D_verify->GetIndex(indexOutVerify);

    featureKernel->SetKernelArg(0,  sizeof(unsigned short),  &nInputWidth);
    featureKernel->SetKernelArg(1,  sizeof(unsigned short),  &nInputHeight);
    featureKernel->SetKernelArg(2,  sizeof(unsigned short),  &kernel_size);
    featureKernel->SetKernelArg(3,  sizeof(unsigned short),  &outputStride);
    featureKernel->SetKernelArg(4,  sizeof(unsigned short),  &numofFeature);
    featureKernel->SetKernelArg(5,  sizeof(nearest), nearest);
    featureKernel->SetKernelArg(6,  sizeof(w), w);
    featureKernel->SetKernelArg(7,  sizeof(boundary_x),   boundary_x);
    featureKernel->SetKernelArg(8,  sizeof(boundary_y),   boundary_y);
    featureKernel->SetKernelArg(9,  sizeof(float),        &sx);
    featureKernel->SetKernelArg(10, sizeof(float),        &sy);
    featureKernel->SetKernelArg(11, sizeof(fDeltaU),      &fDeltaU); 
    featureKernel->SetKernelArg(12, sizeof(fDeltaV),      &fDeltaV); 
    featureKernel->SetKernelArg(15, sizeof(SurfaceIndex), indexOutInfoInput),
    featureKernel->SetKernelArg(16, sizeof(SurfaceIndex), indexOutVerify);

    processKernel->SetKernelArg(0,  sizeof(unsigned short),  &nInputWidth);
    processKernel->SetKernelArg(1,  sizeof(unsigned short),  &nInputHeight);
    processKernel->SetKernelArg(2,  sizeof(unsigned short),  &kernel_size);
    processKernel->SetKernelArg(3,  sizeof(unsigned short),  &outputStride);
    processKernel->SetKernelArg(4,  sizeof(unsigned short),  &numofFeature);
    processKernel->SetKernelArg(5, sizeof(SurfaceIndex),    indexOutVerify);
    processKernel->SetKernelArg(6, sizeof(SurfaceIndex),    indexOutInfoInput);
    processKernel->SetKernelArg(7, sizeof(SurfaceIndex),    indexOutNormal);

    normalizePCAKernel->SetKernelArg(0,  sizeof(unsigned short),  &nInputWidth);
    normalizePCAKernel->SetKernelArg(1,  sizeof(unsigned short),  &nInputHeight);
    normalizePCAKernel->SetKernelArg(2,  sizeof(unsigned short),  &kernel_size);
    normalizePCAKernel->SetKernelArg(3,  sizeof(unsigned short),  &outputStride);
    normalizePCAKernel->SetKernelArg(4,  sizeof(unsigned short),  &numofFeature);
    normalizePCAKernel->SetKernelArg(5, sizeof(SurfaceIndex),    indexOutInfoInput);
    normalizePCAKernel->SetKernelArg(6, sizeof(SurfaceIndex),    indexOutNormal);
    normalizePCAKernel->SetKernelArg(7, sizeof(SurfaceIndex),    indexOutNewFeature);

    return 0;
}

int KCFTracker::getFeatureMaps_gpu(unsigned int vaSurfaceID,  cv::Rect extracted_roi,  int origwidth,  int origheight, int tempWidth, int tempHeight, const int k, CvLSVMFeatureMapCaskade **map)
{
    int width  = tempWidth;
    int height = tempHeight;
    int rawWidth  = origwidth;
    int rawHeight = origheight;
    std::chrono::high_resolution_clock::time_point tmStart, tmStart0;
    std::chrono::high_resolution_clock::time_point tmEnd;
    chrono::duration<double> diffTime;

    tmStart = std::chrono::high_resolution_clock::now();

    int sizeX =  tempWidth  / cell_size;
    int sizeY  = tempHeight / cell_size;
    int px    = 3 * NUM_SECTOR; 
    int p     = px;
    int stringSize = sizeX * p;

    int result;
    int codeSize;
    void *pCommonISACode2;
    FILE* pISA;
    if( NULL == programHogFeature)
    {                  
        pISA = fopen("../../kcfGPU/kcf_featrue_genx.isa", "rb");
        if (pISA == NULL) 
        { 
            printf("Error in loading ISA file."); exit(-1); 
        }
        fseek (pISA, 0, SEEK_END);
        codeSize = ftell (pISA);
        rewind(pISA);
        if(codeSize == 0) 
        {
            printf("Code size is 0."); exit(-1); 
        }
        pCommonISACode2 = (BYTE*) malloc(codeSize);
        if (pCommonISACode2 == NULL)
            exit(-1);
    
        if (fread(pCommonISACode2, 1, codeSize, pISA) != codeSize) 
        {
            printf("Error reading in ISA."); exit(-1);
        }
        fclose(pISA);
 
        result = pCmDev->LoadProgram(pCommonISACode2, codeSize, programHogFeature); 
        if (result != CM_SUCCESS ) 
        { printf("CM LoadProgram error: %d",result); 
            exit(-1); 
        }
        free(pCommonISACode2);
         pCommonISACode2 = NULL;
    }
    // Create Kernel
    if( NULL == featureKernel )
    {
        result = pCmDev->CreateKernel(programHogFeature, CM_KERNEL_FUNCTION(generateFeature) , featureKernel); 
        if (result != CM_SUCCESS ) 
        { 
            printf("CM CreateKernel error: %d",result); exit(-1); 
        }
    }
    if( NULL == processKernel )
    {
        result = pCmDev->CreateKernel(programHogFeature, CM_KERNEL_FUNCTION(process_feature_map) , processKernel); 
        if (result != CM_SUCCESS ) 
        { 
            printf("CM CreateKernel error: %d",result); exit(-1); 
        }
    }
    if( NULL == normalizePCAKernel)
    {
        result = pCmDev->CreateKernel(programHogFeature, CM_KERNEL_FUNCTION(normalizeAndTruncate) , normalizePCAKernel); 
        if (result != CM_SUCCESS ) 
        { 
            printf("CM CreateKernel error: %d",result); exit(-1); 
        }
    }

    
    if( tempWidth  != templateWidth || 
        tempHeight != templateHeight )
    {
        templateWidth  = tempWidth;
        templateHeight = tempHeight;
        previous_roi.x = extracted_roi.x ;
        previous_roi.y = extracted_roi.y ;
        previous_roi.width  = extracted_roi.width ;
        previous_roi.height = extracted_roi.height;

        freeFeatureMapObject(map);
        allocFeatureMapObject(map, sizeX-2, sizeY-2, p+4);
        if (*map == NULL)
            return -1;

        featuremap = (unsigned char *)(*map)->map;
        refreshKernels(rawWidth, rawHeight, extracted_roi, tempWidth, tempHeight);

        std::map<long long, SamplerIndex*>::reverse_iterator iter ;
        for(iter = m_mInputSamplerIndexPool.rbegin(); iter!= m_mInputSamplerIndexPool.rend(); iter++)
        {
            SamplerIndex* pInputSamperIndex = m_mInputSamplerIndexPool[iter->first];
            if( NULL != pInputSamperIndex)
            {
                pSampler8x8->GetIndex(pInputSamperIndex);
                CmSurface2D *pCmSurfaceNV12 =m_pCmSurfaceNV12[iter->first];
                pCmDev->DestroySampler8x8Surface(m_mISurfaceIndexPool[iter->first]);

                SurfaceIndex * index0= NULL;
                 result = pCmDev->CreateSampler8x8Surface(pCmSurfaceNV12, index0, CM_AVS_SURFACE);
                if (result != CM_SUCCESS ) 
                {
                    std::cout<<"CM Sampler8x8Surface error"<<std::endl;
                    return -1;
                }
                         
                m_mInputSamplerIndexPool[iter->first]  = pInputSamperIndex;
                m_mISurfaceIndexPool[iter->first]      = index0;
            }
        }
    }

   // Update the Input SurfaceID;
    SamplerIndex* pInputSamperIndex = m_mInputSamplerIndexPool[vaSurfaceID];
    if( pInputSamperIndex == NULL)
    {
        surfaceInfoS surfaceInfoInput;
        memset(&surfaceInfoInput, 0, sizeof(surfaceInfoS));
        surfaceInfoInput.surfaceID    = vaSurfaceID;
        surfaceInfoInput.bExternal    = TRUE;

        SurfaceIndex* indexInputNV12= NULL;
        CmSurface2D *pCmSurfaceNV12 = setupSurface(pCmDev, surfaceInfoInput);

        m_pCmSurfaceNV12[vaSurfaceID] = pCmSurfaceNV12;

        // Get sampler index for input surface
        SamplerIndex* pInputSamplerIndex = NULL;
        pSampler8x8->GetIndex(pInputSamplerIndex);

        // Get sampler8x8 input surface index
        SurfaceIndex * index0= NULL;
        result = pCmDev->CreateSampler8x8Surface(pCmSurfaceNV12, index0, CM_AVS_SURFACE);
        if (result != CM_SUCCESS ) 
        {
            return -1;
        }
        m_mInputSamplerIndexPool[vaSurfaceID]  = pInputSamplerIndex;
        m_mISurfaceIndexPool[vaSurfaceID]      = index0;
    }
    //update kernel   
    // Calculate the offset of left corner(sx, sy)
    float sx = (extracted_roi.x * 1.0)/rawWidth;
    float sy = (extracted_roi.y * 1.0)/rawHeight;
    float fDeltaU = 1.0f * extracted_roi.width / ((float) width *rawWidth);
    float fDeltaV = 1.0f * extracted_roi.height / ((float)height*rawHeight);  

    featureKernel->SetKernelArg(9,  sizeof(float),        &sx);
    featureKernel->SetKernelArg(10, sizeof(float),        &sy);
    featureKernel->SetKernelArg(11, sizeof(fDeltaU),      &fDeltaU); 
    featureKernel->SetKernelArg(12, sizeof(fDeltaV),      &fDeltaV); 
    featureKernel->SetKernelArg(13,  sizeof(SurfaceIndex),  m_mISurfaceIndexPool[vaSurfaceID]);
    featureKernel->SetKernelArg(14,  sizeof(SamplerIndex),  m_mInputSamplerIndexPool[vaSurfaceID]);

    unsigned char walkerPattern =0;
    CmEvent* pCmEvent = NULL ;
    tmStart0 = std::chrono::high_resolution_clock::now();     
    CmThreadSpace *pTS= NULL;

    result = pCmDev->CreateThreadSpace(threadswidth, threadsheight, pTS); 
    if (result != CM_SUCCESS ) 
    { 
        printf("CM Create ThreadSpace error: %d",result); exit(-1); 
    }

    result = pTS->SelectMediaWalkingPattern(CM_WALK_HORIZONTAL);
    if  (result != CM_SUCCESS)
    {
        printf("CM Create ThreadPattern error: %d",result); exit(-1); 
    }

    if (pCmQueueGetFeatures == NULL)
    {
        result = pCmDev->CreateQueue( pCmQueueGetFeatures );
        if (result != CM_SUCCESS ) {
            perror("CM CreateQueue error");
            return -1;
        }
    }

    if( NULL == pKernelArrayGetFeatures)
    {
        result = pCmDev->CreateTask(pKernelArrayGetFeatures);
        if (result != CM_SUCCESS ) 
        { 
            printf("CmDevice CreateTask error: %d",result); exit(-1); 
        }
        result = pKernelArrayGetFeatures-> AddKernel(featureKernel);
        if (result != CM_SUCCESS ) 
        { 
            printf("CmDevice AddKernel error: %d",result); 
            return -1; 
        }
        
         result = pKernelArrayGetFeatures->AddSync();
        if (result != CM_SUCCESS ) 
        { 
            printf("CmDevice AddSync failed error: %d",result); 
            return -1; 
        }
        
        result = pKernelArrayGetFeatures-> AddKernel(processKernel);
        if (result != CM_SUCCESS ) 
        { 
            printf("CmDevice AddKernel error: %d",result); 
            return -1; 
        }
        
         result = pKernelArrayGetFeatures->AddSync();
        if (result != CM_SUCCESS ) 
        { 
            printf("CmDevice AddSync failed error: %d",result); 
            return -1; 
        }
        
        result = pKernelArrayGetFeatures-> AddKernel(normalizePCAKernel);
        if (result != CM_SUCCESS ) 
        { 
            printf("CmDevice AddKernel error: %d",result); 
            return -1; 
        }
        
    }
    result = pCmQueueGetFeatures->Enqueue(pKernelArrayGetFeatures, pCmEvent, pTS);
    if (result != CM_SUCCESS ) 
    { 
        printf("CmDevice enqueue xxxx error: %d\r\n",result); 
        exit(-1);
    }

#ifdef EU_EXECUTION_TIME
    UINT64 executionTime = 0;
    DWORD dwTimeOutMs = -1;
    result = pCmEvent->WaitForTaskFinished(dwTimeOutMs);

  
    result = pCmEvent->GetExecutionTime( executionTime );
    if (result != CM_SUCCESS ) 
    {
        printf("CM GetExecutionTime error"); 
        exit(-1); 
    }
    else 
    { 
        printf("  >**** Feature extraction takes  %0.2f(ms)\n", executionTime/1000000.0  ); 
    }
 
    tmEnd = std::chrono::high_resolution_clock::now();
    diffTime  = tmEnd   - tmStart0;
    std::cout<< "  >GetFeatures Stage feature/normalize/PCA takes:" << diffTime.count()*1000 <<"(ms)"<<std::endl;
#endif
    result = pCmSurface2D_newFeature->ReadSurface( (unsigned char *)(*map)->map, pCmEvent );
    if (result != CM_SUCCESS )	  { 
        std::cout<<"CM ReadSurface error:"<<result<<std::endl; 
    }
    
    pCmDev->DestroyThreadSpace(pTS);
    pCmQueueGetFeatures->DestroyEvent(pCmEvent);  
    //pCmDev->DestroyTask(pKernelArrayGetFeatures);

  
    tmEnd = std::chrono::high_resolution_clock::now();
    diffTime  = tmEnd   - tmStart;
   // std::cout<< "  >Get features-0 take:" << diffTime.count()*1000 <<"(ms)"<<std::endl;
   
    return 0;
}


#define AVS_MODE      0
#define UNORM_MODE    1
#define SAMPLE16_MODE 2


#define HORIZONTAL_WALKER 0
#define VERTICAL_WALSER   1

#define CV_PI   3.1415926535897932384626433832795
#define CV_MAX_LOCAL_DFT_SIZE  (1 << 15)
#define CV_SWAP(a,b,t) ((t) = (a), (a) = (b), (b) = (t))

static unsigned char bitrevTab[] =
{
  0x00,0x80,0x40,0xc0,0x20,0xa0,0x60,0xe0,0x10,0x90,0x50,0xd0,0x30,0xb0,0x70,0xf0,
  0x08,0x88,0x48,0xc8,0x28,0xa8,0x68,0xe8,0x18,0x98,0x58,0xd8,0x38,0xb8,0x78,0xf8,
  0x04,0x84,0x44,0xc4,0x24,0xa4,0x64,0xe4,0x14,0x94,0x54,0xd4,0x34,0xb4,0x74,0xf4,
  0x0c,0x8c,0x4c,0xcc,0x2c,0xac,0x6c,0xec,0x1c,0x9c,0x5c,0xdc,0x3c,0xbc,0x7c,0xfc,
  0x02,0x82,0x42,0xc2,0x22,0xa2,0x62,0xe2,0x12,0x92,0x52,0xd2,0x32,0xb2,0x72,0xf2,
  0x0a,0x8a,0x4a,0xca,0x2a,0xaa,0x6a,0xea,0x1a,0x9a,0x5a,0xda,0x3a,0xba,0x7a,0xfa,
  0x06,0x86,0x46,0xc6,0x26,0xa6,0x66,0xe6,0x16,0x96,0x56,0xd6,0x36,0xb6,0x76,0xf6,
  0x0e,0x8e,0x4e,0xce,0x2e,0xae,0x6e,0xee,0x1e,0x9e,0x5e,0xde,0x3e,0xbe,0x7e,0xfe,
  0x01,0x81,0x41,0xc1,0x21,0xa1,0x61,0xe1,0x11,0x91,0x51,0xd1,0x31,0xb1,0x71,0xf1,
  0x09,0x89,0x49,0xc9,0x29,0xa9,0x69,0xe9,0x19,0x99,0x59,0xd9,0x39,0xb9,0x79,0xf9,
  0x05,0x85,0x45,0xc5,0x25,0xa5,0x65,0xe5,0x15,0x95,0x55,0xd5,0x35,0xb5,0x75,0xf5,
  0x0d,0x8d,0x4d,0xcd,0x2d,0xad,0x6d,0xed,0x1d,0x9d,0x5d,0xdd,0x3d,0xbd,0x7d,0xfd,
  0x03,0x83,0x43,0xc3,0x23,0xa3,0x63,0xe3,0x13,0x93,0x53,0xd3,0x33,0xb3,0x73,0xf3,
  0x0b,0x8b,0x4b,0xcb,0x2b,0xab,0x6b,0xeb,0x1b,0x9b,0x5b,0xdb,0x3b,0xbb,0x7b,0xfb,
  0x07,0x87,0x47,0xc7,0x27,0xa7,0x67,0xe7,0x17,0x97,0x57,0xd7,0x37,0xb7,0x77,0xf7,
  0x0f,0x8f,0x4f,0xcf,0x2f,0xaf,0x6f,0xef,0x1f,0x9f,0x5f,0xdf,0x3f,0xbf,0x7f,0xff
};

static const double DFTTab[][2] =
{
{ 1.00000000000000000, 0.00000000000000000 },
{-1.00000000000000000, 0.00000000000000000 },
{ 0.00000000000000000, 1.00000000000000000 },
{ 0.70710678118654757, 0.70710678118654746 },
{ 0.92387953251128674, 0.38268343236508978 },
{ 0.98078528040323043, 0.19509032201612825 },
{ 0.99518472667219693, 0.09801714032956060 },
{ 0.99879545620517241, 0.04906767432741802 },
{ 0.99969881869620425, 0.02454122852291229 },
{ 0.99992470183914450, 0.01227153828571993 },
{ 0.99998117528260111, 0.00613588464915448 },
{ 0.99999529380957619, 0.00306795676296598 },
{ 0.99999882345170188, 0.00153398018628477 },
{ 0.99999970586288223, 0.00076699031874270 },
{ 0.99999992646571789, 0.00038349518757140 },
{ 0.99999998161642933, 0.00019174759731070 },
{ 0.99999999540410733, 0.00009587379909598 },
{ 0.99999999885102686, 0.00004793689960307 },
{ 0.99999999971275666, 0.00002396844980842 },
{ 0.99999999992818922, 0.00001198422490507 },
{ 0.99999999998204725, 0.00000599211245264 },
{ 0.99999999999551181, 0.00000299605622633 },
{ 0.99999999999887801, 0.00000149802811317 },
{ 0.99999999999971945, 0.00000074901405658 },
{ 0.99999999999992983, 0.00000037450702829 },
{ 0.99999999999998246, 0.00000018725351415 },
{ 0.99999999999999567, 0.00000009362675707 },
{ 0.99999999999999889, 0.00000004681337854 },
{ 0.99999999999999978, 0.00000002340668927 },
{ 0.99999999999999989, 0.00000001170334463 },
{ 1.00000000000000000, 0.00000000585167232 },
{ 1.00000000000000000, 0.00000000292583616 }
};

#define BitRev(i,shift) \
   ((int)((((unsigned)bitrevTab[(i)&255] << 24)+ \
           ((unsigned)bitrevTab[((i)>> 8)&255] << 16)+ \
           ((unsigned)bitrevTab[((i)>>16)&255] <<  8)+ \
           ((unsigned)bitrevTab[((i)>>24)])) >> (shift)))



int KCFTracker::DFTFactorize( int n, int* factors )
{
    int nf = 0, f, i, j;

    if( n <= 5 )
    {
        factors[0] = n;
        return 1;
    }

    f = (((n - 1)^n)+1) >> 1;
    if( f > 1 )
    {
        factors[nf++] = f;
        n = f == n ? 1 : n/f;
    }

    for( f = 3; n > 1; )
    {
        int d = n/f;
        if( d*f == n )
        {
            factors[nf++] = f;
            n = d;
        }
        else
        {
            f += 2;
            if( f*f > n )
                break;
        }
    }

    if( n > 1 )
        factors[nf++] = n;

    f = (factors[0] & 1) == 0;
    for( i = f; i < (nf+f)/2; i++ )
        CV_SWAP( factors[i], factors[nf-i-1+f], j );

    return nf;
}

void KCFTracker::DFTInit( int n0, int nf, int* factors, int* itab, int elem_size, float* _wave, int inv_itab )
{
    int digits[34], radix[34];
    int n = factors[0], m = 0;
    int* itab0 = itab;
    int i, j, k;
    double w[2], w1[2];
    double t;

    if( n0 <= 5 )
    {
        itab[0] = 0;
        itab[n0-1] = n0-1;

        if( n0 != 4 )
        {
            for( i = 1; i < n0-1; i++ )
                itab[i] = i;
        }
        else
        {
            itab[1] = 2;
            itab[2] = 1;
        }
        if( n0 == 5 )
        {
            _wave[0] = 1.0f;
            _wave[1] = 0.0f;
        }
        if( n0 != 4 )
            return;
        m = 2;
    }
    else
    {
        // radix[] is initialized from index 'nf' down to zero
        assert (nf < 34);
        radix[nf] = 1;
        digits[nf] = 0;
        for( i = 0; i < nf; i++ )
        {
            digits[i] = 0;
            radix[nf-i-1] = radix[nf-i]*factors[nf-i-1];
        }

        if( inv_itab && factors[0] != factors[nf-1] )
            itab = (int*)_wave;

        if( (n & 1) == 0 )
        {
            int a = radix[1], na2 = n*a>>1, na4 = na2 >> 1;
            for( m = 0; (unsigned)(1 << m) < (unsigned)n; m++ )
                ;
            if( n <= 2  )
            {
                itab[0] = 0;
                itab[1] = na2;
            }
            else if( n <= 256 )
            {
                int shift = 10 - m;
                for( i = 0; i <= n - 4; i += 4 )
                {
                    j = (bitrevTab[i>>2]>>shift)*a;
                    itab[i] = j;
                    itab[i+1] = j + na2;
                    itab[i+2] = j + na4;
                    itab[i+3] = j + na2 + na4;
                }
            }
            else
            {
                int shift = 34 - m;
                for( i = 0; i < n; i += 4 )
                {
                    int i4 = i >> 2;
                    j = BitRev(i4,shift)*a;
                    itab[i] = j;
                    itab[i+1] = j + na2;
                    itab[i+2] = j + na4;
                    itab[i+3] = j + na2 + na4;
                }
            }

            digits[1]++;

            if( nf >= 2 )
            {
                for( i = n, j = radix[2]; i < n0; )
                {
                    for( k = 0; k < n; k++ )
                        itab[i+k] = itab[k] + j;
                    if( (i += n) >= n0 )
                        break;
                    j += radix[2];
                    for( k = 1; ++digits[k] >= factors[k]; k++ )
                    {
                        digits[k] = 0;
                        j += radix[k+2] - radix[k];
                    }
                }
            }
        }
        else
        {
            for( i = 0, j = 0;; )
            {
                itab[i] = j;
                if( ++i >= n0 )
                    break;
                j += radix[1];
                for( k = 0; ++digits[k] >= factors[k]; k++ )
                {
                    digits[k] = 0;
                    j += radix[k+2] - radix[k];
                }
            }
        }

        if( itab != itab0 )
        {
            itab0[0] = 0;
            for( i = n0 & 1; i < n0; i += 2 )
            {
                int k0 = itab[i];
                int k1 = itab[i+1];
                itab0[k0] = i;
                itab0[k1] = i+1;
            }
        }
    }

    if( (n0 & (n0-1)) == 0 )
    {
        w[0] = w1[0] =  DFTTab[m][0];
        w[1] = w1[1] = -DFTTab[m][1];
    }
    else
    {
        t = -CV_PI*2/n0;
        w[1] = w1[1] = sin(t);
        w[0] = w1[0] = sqrtf(1. - w1[1]*w1[1]);
    }
    n = (n0+1)/2;

    float* wave = _wave;

    wave[0] = 1.f;
    wave[1] = 0.f;

    if( (n0 & 1) == 0 )
    {
        wave[2*n]   = -1.0f;
        wave[2*n+1] =  0.0f;
    }

    for( i = 1; i < n; i++ )
    {
        wave[2*i]   = w[0];
        wave[2*i+1] = w[1];
        wave[2*(n0-i)]   =  w[0];
        wave[2*(n0-i)+1] = -w[1];

        t    = w[0]*w1[0] - w[1]*w1[1];
        w[1] = w[0]*w1[1] + w[1]*w1[0];
        w[0] = t;
    }
}


int KCFTracker::Call_DFT_FWD_H(CmDevice* pCmDev, CmProgram* program, CmKernel* kernel, CmSurface2D*  pInputSurf, CmBufferUP *pParamSurfHUP, CmBuffer*  pOutputSurf, sPerfProfileParam sInParamConfig, int nf, float *fExetime, CmThreadSpace** pTS)
{

    int  result;
    int BlockWidth  = sInParamConfig.usPitchX;
    int BlockHeight = 1;

    // Find # of threads based on frame size in pixel, block size is 16x4 pixels. 
    int threadswidth  = sInParamConfig.usPitchY; 
    int threadsheight = sInParamConfig.usPitchZ;

    kernel->SetThreadCount(threadswidth * threadsheight);

    pCmDev->CreateThreadSpace(threadswidth, threadsheight, *pTS);

    if (sInParamConfig.usWalkerPattern == HORIZONTAL_WALKER)
    {
        (*pTS)->SelectMediaWalkingPattern(CM_WALK_HORIZONTAL);
    }
    else
    {
        (*pTS)->SelectMediaWalkingPattern(CM_WALK_VERTICAL);
    }

    SurfaceIndex * index0 = NULL;
    SurfaceIndex * index1 = NULL;
    SurfaceIndex * index2 = NULL;

    pInputSurf->GetIndex(index0);
    pOutputSurf->GetIndex(index1);
    pParamSurfHUP->GetIndex(index2);

    kernel->SetKernelArg(0, sizeof(SurfaceIndex), index0);
    kernel->SetKernelArg(1, sizeof(SurfaceIndex), index1);
    kernel->SetKernelArg(2, sizeof(SurfaceIndex), index2);

    kernel->SetKernelArg(3, sizeof(sInParamConfig.usPitchX),  &sInParamConfig.usPitchX); 
    kernel->SetKernelArg(4, sizeof(sInParamConfig.usPitchX),  &sInParamConfig.usPitchY); 

    sInParamConfig.fScale = 1.0f;
    kernel->SetKernelArg(5, sizeof(sInParamConfig.fScale),    &sInParamConfig.fScale); 
    kernel->SetKernelArg(6, sizeof(int),                      &nf); 
    kernel->SetKernelArg(7, sizeof(sInParamConfig.ucChannel), &sInParamConfig.ucChannel); 
    kernel->SetKernelArg(8, sizeof(bool),                     &sInParamConfig.bInv);


    // Add a kernel to kernel array
    result = kernel->AssociateThreadSpace(*pTS);
    if (result != CM_SUCCESS ) {
        printf("kernel AssociateThreadSpace error");
        return -1; 
    }
    return 0;
}


int  KCFTracker::Call_DFT_FWD_V(CmDevice* pCmDev, CmProgram* program, CmKernel* kernel, CmBuffer*  pInputSurf, CmBufferUP *ParamBufferUP, CmBuffer* pOutputSurf, sPerfProfileParam sInParamConfig, int nf, float *fExetime, CmThreadSpace** pTS)
{

    int  result;

    // Find # of threads based on frame size in pixel, block size is 16x4 pixels. 
    int threadswidth  = sInParamConfig.usPitchX >> 1; 
    int threadsheight = sInParamConfig.usPitchZ;

    kernel->SetThreadCount(threadswidth * threadsheight);

    pCmDev->CreateThreadSpace(threadswidth, threadsheight, *pTS);

    if (sInParamConfig.usWalkerPattern == HORIZONTAL_WALKER)
    {
        (*pTS)->SelectMediaWalkingPattern(CM_WALK_HORIZONTAL);	
    }
    else
    {
        (*pTS)->SelectMediaWalkingPattern(CM_WALK_VERTICAL);
    }

    SurfaceIndex * index0 = NULL;
    SurfaceIndex * index1 = NULL;
    SurfaceIndex * index2 = NULL;

    pInputSurf->GetIndex(index0);
    pOutputSurf->GetIndex(index1);
    ParamBufferUP->GetIndex(index2);

    kernel->SetKernelArg(0, sizeof(SurfaceIndex), index0);
    kernel->SetKernelArg(1, sizeof(SurfaceIndex), index1);
    kernel->SetKernelArg(2, sizeof(SurfaceIndex), index2);

    kernel->SetKernelArg(3, sizeof(sInParamConfig.usPitchX),  &sInParamConfig.usPitchX); 
    kernel->SetKernelArg(4, sizeof(sInParamConfig.usPitchX),  &sInParamConfig.usPitchY); 

    kernel->SetKernelArg(5, sizeof(sInParamConfig.fScale),    &sInParamConfig.fScale); 
    kernel->SetKernelArg(6, sizeof(int),                      &nf); 
    kernel->SetKernelArg(7, sizeof(bool),                     &sInParamConfig.bInv);

    // Add a kernel to kernel array
    result = kernel->AssociateThreadSpace(*pTS);
    if (result != CM_SUCCESS ) {
        printf("kernel AssociateThreadSpace error");
        return -1; 
    }
    return 0;
}

int  KCFTracker::Call_MulSpec(CmDevice* pCmDev, CmProgram* program, CmKernel* kernel, CmBuffer*  pInputSurf0, CmBuffer*  pInputSurf1, CmSurface2D*  pOutputSurf, sPerfProfileParam sInParamConfig, float *fExetime, CmThreadSpace** pTS)
{
    ////////////////////////////// CMRT specific code //////////////////////////////
    // Create a CM Device
    int  result;

    // Find # of threads based on frame size in pixel, block size is 16x4 pixels. 
    int threadswidth  = (sInParamConfig.usOutWidth  + 7) / 8;
    int threadsheight =  sInParamConfig.usOutHeight;

    kernel->SetThreadCount(threadswidth * threadsheight);

    pCmDev->CreateThreadSpace(threadswidth, threadsheight, *pTS);

    if (sInParamConfig.usWalkerPattern == HORIZONTAL_WALKER)
    {
        (*pTS)->SelectMediaWalkingPattern(CM_WALK_HORIZONTAL);	
    }
    else
    {
        (*pTS)->SelectMediaWalkingPattern(CM_WALK_VERTICAL);
    }

    SurfaceIndex * index0 = NULL;
    SurfaceIndex * index1 = NULL;
    SurfaceIndex * index2 = NULL;

    pInputSurf0->GetIndex(index0);
    pInputSurf1->GetIndex(index1);
    pOutputSurf->GetIndex(index2);

    kernel->SetKernelArg(0, sizeof(SurfaceIndex), index0);
    kernel->SetKernelArg(1, sizeof(SurfaceIndex), index1);
    kernel->SetKernelArg(2, sizeof(SurfaceIndex), index2);

    unsigned int uPitch = sInParamConfig.usPitchX * sInParamConfig.usPitchY * 8;
    kernel->SetKernelArg(3, sizeof(uPitch),  &uPitch); 

    // Add a kernel to kernel array
    result = kernel->AssociateThreadSpace(*pTS);
    if (result != CM_SUCCESS ) {
        printf("kernel AssociateThreadSpace error");
        return -1; 
    }
    return 0;
}

int  KCFTracker::Call_ImageSum(CmDevice* pCmDev, CmProgram* program, CmKernel* kernel, CmBuffer*  pInputSurf0, CmBuffer*  pOutputSurf, sPerfProfileParam sInParamConfig, float *fExetime, CmThreadSpace** pTS)
{
    ////////////////////////////// CMRT specific code //////////////////////////////
    // Create a CM Device
    UINT version = 0;
    int  result;

    // Find # of threads based on frame size in pixel, block size is 16x4 pixels. 
    int threadswidth  = (sInParamConfig.usOutWidth  + 7) / 8;
    int threadsheight =  1;

    //CmThreadSpace* pTS;
    kernel->SetThreadCount(threadswidth * threadsheight);

    pCmDev->CreateThreadSpace(threadswidth, threadsheight, *pTS);

    if (sInParamConfig.usWalkerPattern == HORIZONTAL_WALKER)
    {
        (*pTS)->SelectMediaWalkingPattern(CM_WALK_HORIZONTAL);	
    }
    else
    {
        (*pTS)->SelectMediaWalkingPattern(CM_WALK_VERTICAL);
    }

    SurfaceIndex * index0 = NULL;
    SurfaceIndex * index1 = NULL;

    pInputSurf0->GetIndex(index0);
    pOutputSurf->GetIndex(index1);

    kernel->SetKernelArg(0, sizeof(SurfaceIndex), index0);
    kernel->SetKernelArg(1, sizeof(SurfaceIndex), index1);

    unsigned int uPitch = sInParamConfig.usPitchX * sInParamConfig.usPitchY * 8;
    kernel->SetKernelArg(2, sizeof(uPitch),  &uPitch); 
    kernel->SetKernelArg(3, sizeof(sInParamConfig.usPitchZ),  &sInParamConfig.usPitchZ); 

    result = kernel->AssociateThreadSpace(*pTS);
    if (result != CM_SUCCESS ) {
        printf("kernel AssociateThreadSpace error");
        return -1; 
    }
    return 0;
}

int KCFTracker::Call_SrcImageSum(CmDevice* pCmDev, CmProgram* program, CmSurface2D*  pInputSurf0, CmSurface2D*  pInputSurf1, CmBufferUP* pOutputBufUp, sPerfProfileParam sInParamConfig, float *fExetime)
{
    UINT version = 0;
    int  result;

   // CmKernel* kernel = NULL;    
   if(NULL == kernelSrcImageSum){
       result = pCmDev->CreateKernel(program, CM_KERNEL_FUNCTION(SrcImageSum) , kernelSrcImageSum); 
        if (result != CM_SUCCESS ) {
            perror("CM CreateKernel error");
            return -1;
        }  
    }


    // Find # of threads based on frame size in pixel, block size is 16x4 pixels. 
    int threadswidth  = (sInParamConfig.usInWidth + 7) / 8;
    int threadsheight =  1;

    CmThreadSpace* pTS;
    kernelSrcImageSum->SetThreadCount(threadswidth * threadsheight);

    pCmDev->CreateThreadSpace(threadswidth, threadsheight, pTS);

    if (sInParamConfig.usWalkerPattern == HORIZONTAL_WALKER)
    {
        pTS->SelectMediaWalkingPattern(CM_WALK_HORIZONTAL);	
    }
    else
    {
        pTS->SelectMediaWalkingPattern(CM_WALK_VERTICAL);
    }

    SurfaceIndex * index0 = NULL;
    SurfaceIndex * index1 = NULL;
    SurfaceIndex * index2 = NULL;

    pInputSurf0->GetIndex(index0);
    pInputSurf1->GetIndex(index1);
    pOutputBufUp->GetIndex(index2);

    kernelSrcImageSum->SetKernelArg(0, sizeof(SurfaceIndex), index0);
    kernelSrcImageSum->SetKernelArg(1, sizeof(SurfaceIndex), index1);
    kernelSrcImageSum->SetKernelArg(2, sizeof(SurfaceIndex), index2);
    kernelSrcImageSum->SetKernelArg(3, sizeof(sInParamConfig.usPitchZ),  &sInParamConfig.usPitchZ); 


    // Create a task queue
    if (pCmQueueGaussianCorrection == NULL)
    {
        result = pCmDev->CreateQueue( pCmQueueGaussianCorrection );
        if (result != CM_SUCCESS ) {
            perror("CM CreateQueue error");
            return -1;
        }
    }

    CmTask *pKernelArrayTmp = NULL;

    // Create a task (container) to be put in the task queue
    result = pCmDev->CreateTask(pKernelArrayTmp);
    if (result != CM_SUCCESS ) {
        printf("CmDevice CreateTask error");
        return -1;
    }

    // Add a kernel to kernel array
    result = pKernelArrayTmp-> AddKernel (kernelSrcImageSum);
    if (result != CM_SUCCESS ) {
        printf("CmDevice AddKernel error");
        return -1; 
    }

    std::chrono::high_resolution_clock::time_point tmStart2;
    std::chrono::high_resolution_clock::time_point tmEnd2;
    std::chrono::duration<double> diffTime;

    // Put kernel array into task queue to be executed on GPU
    CmEvent* e = NULL;
    result = pCmQueueGaussianCorrection->Enqueue(pKernelArrayTmp, e, pTS);
    if (result != CM_SUCCESS ) {
        printf("Call_SrcImageSum: CmDevice enqueue error");
        return -1;
    }
    tmStart2 = std::chrono::high_resolution_clock::now();

    // Get kernel output data
    //result = pOutputImageSurf->ReadSurface(OutputBuffer, e);
    result = e->WaitForTaskFinished();
    if (result != CM_SUCCESS ) {
        printf("CM ReadSurface error");
        return -1;
    }
    tmEnd2 = std::chrono::high_resolution_clock::now();
    diffTime  = tmEnd2   - tmStart2;
    //std::cout<< "  Wait  takes: :" << diffTime.count()*1000 <<"(ms)"<<std::endl;

#ifdef EU_EXECUTION_TIME
    UINT64 executionTime = 0, totalTime = 0;

    result = e->GetExecutionTime( executionTime );
    if (result != CM_SUCCESS ) {
        printf("CM GetExecutionTime error");
        return result;
    }
    else
    {
        printf("  >Call_SrcImageSum execution time =  %0.2fms\n", executionTime / 1000000.0 );
    }
#endif
        pCmDev->DestroyThreadSpace(pTS);
    pCmQueueGaussianCorrection->DestroyEvent(e);
    // Destroy a task (container) after using it
    pCmDev->DestroyTask(pKernelArrayTmp);
    return result;
}


int  KCFTracker::Call_FianlExpImageSum(CmDevice* pCmDev, CmProgram* program, CmBuffer*  pInputSurf0, unsigned char *InputBuffer1, unsigned char *OutputBuffer, sPerfProfileParam sInParamConfig, float *fExetime)
{
    UINT version = 0;
    int  result;
    if( NULL == kernelFinalExp){
        result = pCmDev->CreateKernel(program, CM_KERNEL_FUNCTION(ImageExpSum) , kernelFinalExp); 
        if (result != CM_SUCCESS ) {
            perror("CM CreateKernel error");
            return -1;
        }
    }

    if (pInputSumSurf == NULL)
    {
        result = pCmDev->CreateBuffer( 8 * sizeof(float), pInputSumSurf);
        if (result != CM_SUCCESS ){
            printf("CM CreateSurface2D error for input surface 0\n");
            return -1;
        }
    }

    result = pInputSumSurf->WriteSurface(InputBuffer1, NULL);
    if (result != CM_SUCCESS ) {
        printf("CM WriteSurface error");
        return -1;
    }

    // Create a 2D surface for outputss
    if ((pOutputFinalSurf == NULL) || (sInParamConfig.usPitchX != gPitchX) || (sInParamConfig.usPitchY != gPitchY))
    {
        if( NULL != pOutputFinalSurf){
            pCmDev->DestroySurface(pOutputFinalSurf);
            pOutputFinalSurf = NULL;
        }
        result = pCmDev->CreateBuffer(sInParamConfig.usPitchX * sInParamConfig.usPitchY * sizeof(float), pOutputFinalSurf);
        if (result != CM_SUCCESS ) {
            printf("CM CreateSurface2D error for output surface 1 \n");
            return -1;
        }
    }
    // Find # of threads based on frame size in pixel, block size is 16x4 pixels. 
    int threadswidth  = (sInParamConfig.usOutWidth + 15) / 16;
    int threadsheight = 1;

    CmThreadSpace* pTS;
    kernelFinalExp->SetThreadCount(threadswidth * threadsheight);

    pCmDev->CreateThreadSpace(threadswidth, threadsheight, pTS);

    if (sInParamConfig.usWalkerPattern == HORIZONTAL_WALKER)
    {
        pTS->SelectMediaWalkingPattern(CM_WALK_HORIZONTAL);	
    }
    else
    {
        pTS->SelectMediaWalkingPattern(CM_WALK_VERTICAL);
    }

    SurfaceIndex * index0 = NULL;
    SurfaceIndex * index1 = NULL;
    SurfaceIndex * index2 = NULL;

    pInputSurf0->GetIndex(index0);
    pInputSumSurf->GetIndex(index1);
    pOutputFinalSurf->GetIndex(index2);

    kernelFinalExp->SetKernelArg(0, sizeof(SurfaceIndex), index0);
    kernelFinalExp->SetKernelArg(1, sizeof(SurfaceIndex), index1);
    kernelFinalExp->SetKernelArg(2, sizeof(SurfaceIndex), index2);
    kernelFinalExp->SetKernelArg(3, sizeof(sInParamConfig.usPitchX),  &sInParamConfig.usPitchX); 
    kernelFinalExp->SetKernelArg(4, sizeof(sInParamConfig.usPitchY),  &sInParamConfig.usPitchY); 
    kernelFinalExp->SetKernelArg(5, sizeof(sInParamConfig.usPitchZ),  &sInParamConfig.usPitchZ); 

    float fSigma = 0.6;
    kernelFinalExp->SetKernelArg(6, sizeof(float), &fSigma); 


    // Create a task queue
    if (pCmQueueGaussianCorrection == NULL)
    {
        result = pCmDev->CreateQueue( pCmQueueGaussianCorrection );
        if (result != CM_SUCCESS ) {
            perror("CM CreateQueue error");
            return -1;
        }
    }

    CmTask *pKernelArray = NULL;

    // Create a task (container) to be put in the task queue
    result = pCmDev->CreateTask(pKernelArray);
    if (result != CM_SUCCESS ) {
        printf("CmDevice CreateTask error");
        return -1;
    }

    // Add a kernel to kernel array
    result = pKernelArray-> AddKernel (kernelFinalExp);
    if (result != CM_SUCCESS ) {
        printf("CmDevice AddKernel error");
        return -1; 
    }

    // Put kernel array into task queue to be executed on GPU
    CmEvent* e = NULL;
    result = pCmQueueGaussianCorrection->Enqueue(pKernelArray, e, pTS);
    if (result != CM_SUCCESS ) {
        printf("Call_FianlExpImageSum: CmDevice enqueue error");
        return -1;
    }


    result = pOutputFinalSurf->ReadSurface(OutputBuffer, e);
    if (result != CM_SUCCESS ) {
        printf("CM ReadSurface error");
    return -1;
    }

#ifdef EU_EXECUTION_TIME
    UINT64 executionTime = 0, totalTime = 0;

    result = e->GetExecutionTime( executionTime );
    if (result != CM_SUCCESS ) {
        printf("CM GetExecutionTime error");
        return result;
    }
    else
    {
        printf("  >Call_FianlExpImageSum execution time =  %0.2fms\n", executionTime / 1000000.0 );
    }
#endif

    // Destroy a task (container) after using it
    pCmDev->DestroyThreadSpace(pTS);
    pCmQueueGaussianCorrection->DestroyEvent(e);
    pCmDev->DestroyTask(pKernelArray);
    return result;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Main program: Find each block's min and max values.
// 


int KCFTracker::gpu_gaussianCorrelation(unsigned char* InputBuffer0, unsigned char *InputBuffer1, unsigned short usWidth, unsigned short usHeight, unsigned char uChannel, 
                                       unsigned short usPitchX, unsigned short usPitchY, unsigned short usPitchZ, unsigned char *Outputbuffer)
{
    int i;

    // Allocate memory
    int inFrameSize;
    if (uChannel == 1)
    {
        inFrameSize = usWidth * usHeight * sizeof(float);
    }
    else
    {
        inFrameSize = usWidth * usHeight * sizeof(float) * uChannel;
    }

    float fExetime = 0;

    // Horizontal DFT 0
    int   factor[8] = {0};
    int   itab[32]  = {0};
    int   elem_size =  8;
    float wave[64]  = {0.0f};

    sInParamConfig.bInv       = 0;
    sInParamConfig.ucChannel  = uChannel;
    sInParamConfig.usInHeight = sInParamConfig.usOutHeight = usHeight;
    sInParamConfig.usInWidth  = sInParamConfig.usOutWidth  = usWidth;
    sInParamConfig.usPitchX   = usPitchX;
    sInParamConfig.usPitchY   = usPitchY;
    sInParamConfig.usPitchZ   = usPitchZ;
    sInParamConfig.fScale     = 1.0f;

    int  result;


    if ((pOutputSurf_H_F_0 == NULL) || (gWidth != sInParamConfig.usOutWidth) || (gHeight != sInParamConfig.usOutHeight))
    {
            if(NULL != pOutputSurf_H_F_0){
                pCmDev->DestroySurface(pOutputSurf_H_F_0);
                pOutputSurf_H_F_0 = NULL;
            }
            result = pCmDev->CreateBuffer( sInParamConfig.usOutWidth * sInParamConfig.usOutHeight * sizeof(float)*2, pOutputSurf_H_F_0);
            if (result != CM_SUCCESS ) {
                printf("CM CreateSurface2D error for output surface 2 \n");
                return -1;
            }
    }

    if ((pInputSurf0 == NULL) || (pInputSurf1 == NULL) || (gWidth != sInParamConfig.usOutWidth) || (gHeight != sInParamConfig.usOutHeight))
    {
        if(NULL != pInputSurf0){
           pCmDev->DestroySurface(pInputSurf0);
           pInputSurf0 = NULL;
        }
        if(NULL != pInputSurf1){
           pCmDev->DestroySurface(pInputSurf1);
           pInputSurf1 = NULL;
        }

        if (sInParamConfig.bInv == 0)
        {
            result = pCmDev->CreateSurface2D( sInParamConfig.usInWidth, sInParamConfig.usInHeight,  CM_SURFACE_FORMAT_A8R8G8B8, pInputSurf0);
            result = pCmDev->CreateSurface2D( sInParamConfig.usInWidth, sInParamConfig.usInHeight,  CM_SURFACE_FORMAT_A8R8G8B8, pInputSurf1);
        }
        else
        {
            result = pCmDev->CreateSurface2D( sInParamConfig.usInWidth*2, sInParamConfig.usInHeight,  CM_SURFACE_FORMAT_A8R8G8B8, pInputSurf0);
            result = pCmDev->CreateSurface2D( sInParamConfig.usInWidth*2, sInParamConfig.usInHeight,  CM_SURFACE_FORMAT_A8R8G8B8, pInputSurf1);	
        }

        if (result != CM_SUCCESS ){
            printf("CM CreateSurface2D error for input surface 1\n");
            return -1;
        }
    }

    result = pInputSurf0->WriteSurface(InputBuffer0, NULL);
    if (result != CM_SUCCESS ) {
        printf("CM WriteSurface error");
        return -1;
    }

    result = pInputSurf1->WriteSurface(InputBuffer1, NULL);
    if (result != CM_SUCCESS ) {
        printf("CM WriteSurface error");
        return -1;
    }

    int ParamSizeULT = 320;

    if (gPitchX != sInParamConfig.usPitchX)
    {
        nfx = DFTFactorize(sInParamConfig.usPitchX, factor);
        DFTInit( sInParamConfig.usPitchX, nfx, factor, itab, elem_size, wave, 0);

        if (pParamBufferULTH != NULL)
        {
            CM_ALIGNED_FREE(pParamBufferULTH);
            pParamBufferULTH = NULL;
            pCmDev->DestroyBufferUP(pParamSurfHUP);
        }

        pParamBufferULTH = (unsigned char *) CM_ALIGNED_MALLOC(ParamSizeULT, 0X1000);
        if (pParamBufferULTH == NULL)
            return -1;
        memcpy(pParamBufferULTH, wave, 64*4);

        for(int i = 0; i < sInParamConfig.usPitchX; i++)
        {
            pParamBufferULTH[64*4 + i] = itab[i];
        }

        memcpy(pParamBufferULTH + 64*4 + 32, factor, 32);
        //gPitchX = sInParamConfig.usPitchX;

        pCmDev->CreateBufferUP(ParamSizeULT, pParamBufferULTH, pParamSurfHUP);
    }

    if (kernel_H_F_0 == NULL)
    {
        result = pCmDev->CreateKernel(programGaussianCorrection, CM_KERNEL_FUNCTION(fftd_h), kernel_H_F_0); 
		if (result != CM_SUCCESS ) {
			perror("CM CreateKernel error");
			return -1;
		}

		result = pKernelArray->AddKernel(kernel_H_F_0);
		result = pKernelArray->AddSync();
		if (result != CM_SUCCESS ) {
			printf("CmDevice H AddKernel error");
			return -1; 
		}
	}
    CmThreadSpace* pTS0;
    Call_DFT_FWD_H(pCmDev, programGaussianCorrection, kernel_H_F_0, pInputSurf0, pParamSurfHUP, pOutputSurf_H_F_0, sInParamConfig, nfx, &fExetime, &pTS0);
  

    // Vertical DFT 0
    if (gPitchY != sInParamConfig.usPitchY)
    {
        memset(factor,0, 8 *sizeof(int));
        memset(itab,  0, 32*sizeof(unsigned char));
        memset(wave,  0, 64*sizeof(float));

        nfy = DFTFactorize(sInParamConfig.usPitchY, factor);
        DFTInit( sInParamConfig.usPitchY, nfy, factor, itab, elem_size, wave, 0);


        if(pParamBufferULTV != NULL){
            CM_ALIGNED_FREE(pParamBufferULTV);
            pParamBufferULTV = NULL;
            pCmDev->DestroyBufferUP(pParamBufferV);

        }
        pParamBufferULTV = (unsigned char *) CM_ALIGNED_MALLOC(ParamSizeULT, 0x1000);
        if (pParamBufferULTV == NULL)
        {
            return -1;
        }

        memset(pParamBufferULTV, 0,    320);
        memcpy(pParamBufferULTV, wave, 64*4);

        for(int i = 0; i < sInParamConfig.usPitchY; i++)
        {
            pParamBufferULTV[64*4 + i] = itab[i];
        }

        memcpy(pParamBufferULTV + 64*4 + 32, factor, 32);
    //  gPitchY = sInParamConfig.usPitchY;

        pCmDev->CreateBufferUP(ParamSizeULT, pParamBufferULTV, pParamBufferV);
    }

    if ((pOutputSurf_V_F_0 == NULL) || (sInParamConfig.usOutWidth != gWidth) || (sInParamConfig.usInHeight != gHeight))
    {
        if(NULL != pOutputSurf_V_F_0){
           pCmDev->DestroySurface(pOutputSurf_V_F_0);
           pOutputSurf_V_F_0 = NULL;
        }
        result = pCmDev->CreateBuffer( sInParamConfig.usOutWidth * sInParamConfig.usOutHeight * sizeof(float)*2, pOutputSurf_V_F_0);
        if (result != CM_SUCCESS ) {
            printf("CM CreateSurface2D error for output surface 3\n");
            return -1;
        }
    }

    if (kernel_V_F_0 == NULL)
    {
        result = pCmDev->CreateKernel(programGaussianCorrection, CM_KERNEL_FUNCTION(fftd_v), kernel_V_F_0); 
        if (result != CM_SUCCESS ) {
            perror("CM CreateKernel error");
            return -1;
        }

        result = pKernelArray->AddKernel(kernel_V_F_0);
        result = pKernelArray->AddSync();
        if (result != CM_SUCCESS ) {
            printf("CmDevice H AddKernel error");
            return -1; 
        }
    }
    CmThreadSpace* pTS1;
    Call_DFT_FWD_V(pCmDev, programGaussianCorrection, kernel_V_F_0, pOutputSurf_H_F_0, pParamBufferV, pOutputSurf_V_F_0, sInParamConfig, nfy, &fExetime, &pTS1);

    // Horizontal DFT 1
    if ((pOutputSurf_H_F_1 == NULL) || (sInParamConfig.usOutWidth != gWidth) || (sInParamConfig.usInHeight != gHeight))
    {
            if(NULL != pOutputSurf_H_F_1){
               pCmDev->DestroySurface(pOutputSurf_H_F_1);
               pOutputSurf_H_F_1 = NULL;
            }
            result = pCmDev->CreateBuffer( sInParamConfig.usOutWidth * sInParamConfig.usOutHeight * sizeof(float)*2, pOutputSurf_H_F_1);
            if (result != CM_SUCCESS ) {
                printf("CM CreateSurface2D error for output surface 4\n");
                return -1;
            }
    }

    if (kernel_H_F_1 == NULL)
    {
        result = pCmDev->CreateKernel(programGaussianCorrection, CM_KERNEL_FUNCTION(fftd_h), kernel_H_F_1); 
        if (result != CM_SUCCESS ) {
            perror("CM CreateKernel error");
            return -1;
        }

        result = pKernelArray->AddKernel(kernel_H_F_1);
        result = pKernelArray->AddSync();

        if (result != CM_SUCCESS ) {
            printf("CmDevice H AddKernel error");
            return -1; 
        }
    }
    CmThreadSpace* pTS2;
    Call_DFT_FWD_H(pCmDev, programGaussianCorrection, kernel_H_F_1, pInputSurf1, pParamSurfHUP, pOutputSurf_H_F_1, sInParamConfig, nfx, &fExetime, &pTS2);

    // Vertical DFT 1
    if ((pOutputSurf_V_F_1 == NULL) || (sInParamConfig.usOutWidth != gWidth) || (sInParamConfig.usInHeight != gHeight))
    {
        if(NULL != pOutputSurf_V_F_1){
           pCmDev->DestroySurface(pOutputSurf_V_F_1);
           pOutputSurf_V_F_1 = NULL;
        }
        result = pCmDev->CreateBuffer( sInParamConfig.usOutWidth * sInParamConfig.usOutHeight * sizeof(float)*2, pOutputSurf_V_F_1);
        if (result != CM_SUCCESS ) {
            printf("CM CreateSurface2D error for output surface 5 \n");
            return -1;
        }
    }

    if (kernel_V_F_1 == NULL)
    {
        result = pCmDev->CreateKernel(programGaussianCorrection, CM_KERNEL_FUNCTION(fftd_v), kernel_V_F_1); 
        if (result != CM_SUCCESS ) {
            perror("CM CreateKernel error");
            return -1;
        }

        result = pKernelArray->AddKernel(kernel_V_F_1);
        result = pKernelArray->AddSync();
        if (result != CM_SUCCESS ) {
            printf("CmDevice H AddKernel error");
            return -1; 
        }
    }
    CmThreadSpace* pTS3;
    Call_DFT_FWD_V(pCmDev, programGaussianCorrection, kernel_V_F_1, pOutputSurf_H_F_1, pParamBufferV, pOutputSurf_V_F_1, sInParamConfig, nfy, &fExetime, &pTS3);

    // Call mulSpectrums
    if ((pMulSpectrumSurf == NULL) || (sInParamConfig.usOutWidth != gWidth) || (sInParamConfig.usOutHeight != gHeight))
    {
        if(NULL != pMulSpectrumSurf){
           pCmDev->DestroySurface(pMulSpectrumSurf);
           pMulSpectrumSurf = NULL;
        }
        result = pCmDev->CreateSurface2D( sInParamConfig.usOutWidth*2, sInParamConfig.usOutHeight, CM_SURFACE_FORMAT_A8R8G8B8, pMulSpectrumSurf);
        if (result != CM_SUCCESS ) {
            printf("CM CreateSurface2D error for output surface 6 \n");
            return -1;
        }
    }

    if (kernel_MulSpec == NULL)
    {
        result = pCmDev->CreateKernel(programGaussianCorrection, CM_KERNEL_FUNCTION(mulSpectrums) , kernel_MulSpec); 

        result = pKernelArray->AddKernel(kernel_MulSpec);
        result = pKernelArray->AddSync();
        if (result != CM_SUCCESS ) {
            printf("CmDevice H AddKernel error");
            return -1; 
        }
    }
    CmThreadSpace* pTS4;
    Call_MulSpec(pCmDev, programGaussianCorrection, kernel_MulSpec, pOutputSurf_V_F_0, pOutputSurf_V_F_1, pMulSpectrumSurf, sInParamConfig, &fExetime, &pTS4);

    // Inv Horizontal DFT
    sInParamConfig.bInv = 1;

    if ((pOutputSurf_H_V_1 == NULL) || (sInParamConfig.usOutWidth != gWidth) || (sInParamConfig.usInHeight != gHeight))
    {
        if(NULL != pOutputSurf_H_V_1){
           pCmDev->DestroySurface(pOutputSurf_H_V_1);
           pOutputSurf_H_V_1 = NULL;
        }

        result = pCmDev->CreateBuffer( sInParamConfig.usOutWidth * sInParamConfig.usOutHeight * sizeof(float)*2, pOutputSurf_H_V_1);
        if (result != CM_SUCCESS ) {
            printf("CM CreateSurface2D error for output surface 7 \n");
            return -1;
        }
    }

    if (kernel_H_I == NULL)
    {
        result = pCmDev->CreateKernel(programGaussianCorrection, CM_KERNEL_FUNCTION(fftd_h), kernel_H_I); 
        if (result != CM_SUCCESS ) {
            perror("CM CreateKernel error");
            return -1;
        }

        result = pKernelArray->AddKernel(kernel_H_I);
        result = pKernelArray->AddSync();
        if (result != CM_SUCCESS ) {
            printf("CmDevice H AddKernel error");
            return -1; 
        }
    }
    CmThreadSpace* pTS_DFT_FWD_H;
    Call_DFT_FWD_H(pCmDev, programGaussianCorrection, kernel_H_I, pMulSpectrumSurf, pParamSurfHUP, pOutputSurf_H_V_1, sInParamConfig, nfx, &fExetime, &pTS_DFT_FWD_H);

    if ((pOutputSurf_V_V_1 == NULL) || (sInParamConfig.usOutWidth != gWidth) || (sInParamConfig.usInHeight != gHeight))
    {
        if(NULL != pOutputSurf_V_V_1){
           pCmDev->DestroySurface(pOutputSurf_V_V_1);
           pOutputSurf_V_V_1 = NULL;
        }
        result = pCmDev->CreateBuffer( sInParamConfig.usOutWidth * sInParamConfig.usOutHeight * sizeof(float)*2, pOutputSurf_V_V_1);
        if (result != CM_SUCCESS ) {
            printf("CM CreateSurface2D error for output surface 8 \n");
            return -1;
        }
    }

    if (kernel_V_I == NULL)
    {
        result = pCmDev->CreateKernel(programGaussianCorrection, CM_KERNEL_FUNCTION(fftd_v), kernel_V_I); 
        if (result != CM_SUCCESS ) {
            perror("CM CreateKernel error");
            return -1;
        }

        result = pKernelArray->AddKernel(kernel_V_I);
        result = pKernelArray->AddSync();
        if (result != CM_SUCCESS ) {
            printf("CmDevice H AddKernel error");
            return -1; 
        }
    }

    sInParamConfig.fScale = 1.0f/(sInParamConfig.usPitchX * sInParamConfig.usPitchY);
    CmThreadSpace* pTS_DFT_FWD_V;
    Call_DFT_FWD_V(pCmDev, programGaussianCorrection, kernel_V_I, pOutputSurf_H_V_1, pParamBufferV, pOutputSurf_V_V_1, sInParamConfig, nfy, &fExetime, &pTS_DFT_FWD_V);

    // INv DFT image Sum
    if ((pOutputInvSumSurf == NULL) || (sInParamConfig.usOutWidth != gWidth) || (sInParamConfig.usInHeight != gHeight))
    {
        if(NULL != pOutputInvSumSurf){
           pCmDev->DestroySurface(pOutputInvSumSurf);
           pOutputInvSumSurf = NULL;
        }
        result = pCmDev->CreateBuffer( sInParamConfig.usPitchX * sInParamConfig.usPitchY * sizeof(float), pOutputInvSumSurf);
        if (result != CM_SUCCESS ) {
            printf("CM CreateSurface2D error for output surface 9 \n");
            return -1;
        }
    }

    if (kernel_ImageSum == NULL)
    {
        result = pCmDev->CreateKernel(programGaussianCorrection, CM_KERNEL_FUNCTION(InvRealSum), kernel_ImageSum); 
        if (result != CM_SUCCESS ) {
            perror("CM CreateKernel error");
            return -1;
        }

        result = pKernelArray->AddKernel(kernel_ImageSum);
        result = pKernelArray->AddSync();
        if (result != CM_SUCCESS ) {
            printf("CmDevice H AddKernel error");
            return -1; 
        }
    }
    CmThreadSpace* pTSImageSum;
    Call_ImageSum(pCmDev, programGaussianCorrection, kernel_ImageSum, pOutputSurf_V_V_1, pOutputInvSumSurf, sInParamConfig, &fExetime, &pTSImageSum);

#ifdef EU_EXECUTION_TIME
    CmEvent* e = NULL;
#else
    CmEvent *e = CM_NO_EVENT;
#endif

    result = pCmQueueGaussianCorrection->Enqueue(pKernelArray, e);
    if (result != CM_SUCCESS ) {
        printf(" CmDevice enqueue error");
        return -1;
    }

#ifdef EU_EXECUTION_TIME
    e->WaitForTaskFinished();
    UINT64 executionTime = 0, totalTime = 0;

    result = e->GetExecutionTime( executionTime );
    if (result != CM_SUCCESS ) {
        printf("CM GetExecutionTime error");
        return result;
    }

    printf("  >**** GaussanCorrection takes  %0.2fms\n", executionTime / 1000000.0 );

    pCmQueueGaussianCorrection->DestroyEvent(e);
#endif


    // Src Image Sum
    #define SUM_VERIFUCATION

    if ((SrcImageSumBuffer == NULL) || (sInParamConfig.usOutWidth != gWidth))
    {
        if(SrcImageSumBuffer != NULL)
        {
            free(SrcImageSumBuffer);
            SrcImageSumBuffer = NULL;
            pCmDev->DestroyBufferUP(pSrcImageSumBufferUp);
        }

        SrcImageSumBuffer = (unsigned char *)CM_ALIGNED_MALLOC(sInParamConfig.usOutWidth * sizeof(float), 0x1000);
        if (SrcImageSumBuffer == NULL)
            return -1;
        pCmDev->CreateBufferUP(sInParamConfig.usOutWidth * sizeof(float), SrcImageSumBuffer, pSrcImageSumBufferUp);
    }

    Call_SrcImageSum(pCmDev, programGaussianCorrection, pInputSurf0, pInputSurf1, pSrcImageSumBufferUp, sInParamConfig, &fExetime);

#ifdef SUM_VERIFUCATION
    float sum[8] = {0};
    float *pSum = (float *)SrcImageSumBuffer;
    for(int i = 0; i < sInParamConfig.usOutWidth; i++)
    {
        sum[0] += *pSum++;
    }
#endif

    // Final exp sum
    Call_FianlExpImageSum(pCmDev, programGaussianCorrection, pOutputInvSumSurf, (unsigned char*)sum, Outputbuffer, sInParamConfig, &fExetime);
    pCmDev->DestroyThreadSpace(pTS0);
    pCmDev->DestroyThreadSpace(pTS1);
    pCmDev->DestroyThreadSpace(pTS2);
    pCmDev->DestroyThreadSpace(pTS3);
    pCmDev->DestroyThreadSpace(pTS4);
    pCmDev->DestroyThreadSpace(pTS_DFT_FWD_H);
    pCmDev->DestroyThreadSpace(pTSImageSum);
    pCmDev->DestroyThreadSpace(pTS_DFT_FWD_V);

    gWidth   = sInParamConfig.usOutWidth;
    gHeight  = sInParamConfig.usOutHeight;
    gPitchX  = sInParamConfig.usPitchX;
    gPitchY  = sInParamConfig.usPitchY;
    gPitchZ  = sInParamConfig.usPitchZ;

    return 0;

}

