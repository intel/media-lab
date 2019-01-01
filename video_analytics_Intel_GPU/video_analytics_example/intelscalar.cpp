#include "intelscalartbl.h"
#include <assert.h>
#include <iostream>
#include <limits>
#include <stdio.h>

#define AVS_MODE      0
#define UNORM_MODE    1
#define SAMPLE16_MODE 2

#define HORIZONTAL_WALKER 0
#define VERTICAL_WALSER   1


int setupAVSSampler(CmDevice       *pCmDev,
                    unsigned short  usOutWidth,
                    unsigned short  usOutHeight,
                    CmSampler8x8   **pSampler8x8)
{
    ////////////////////////////// CMRT specific code //////////////////////////////
    if( NULL == pCmDev ||
        NULL == pSampler8x8)
    {
        std::cout<<"Invalid parameter provided"<<std::endl;
        return -1;
    }
    int result = 0;
    float fDeltaU = 1.0f / (float)usOutWidth;
    float fDeltaV = 1.0f / (float)usOutHeight;

    CM_SAMPLER_8X8_DESCR samplerStateDescr;
    CM_AVS_STATE_MSG AvsStateMsg;
    memset(&AvsStateMsg, 0, sizeof(CM_AVS_STATE_MSG));
    samplerStateDescr.avs = &AvsStateMsg;

    CM_AVS_NONPIPLINED_STATE AvsState;
    memset(&AvsState, 0, sizeof(CM_AVS_NONPIPLINED_STATE));
    samplerStateDescr.avs->AvsState= &AvsState;

    float dum = 0.125f;
    
    // config AVS 
    samplerStateDescr.stateType      = CM_SAMPLER8X8_AVS;// AVS
    samplerStateDescr.avs->AVSTYPE   = 0;                // Adaptive
    samplerStateDescr.avs->BypassIEF = 1;                // True: ignored for BDW
    samplerStateDescr.avs->EightTapAFEnable	= 0x00;							// 01: Enable 8-tap Adaptive filter on G-channel. 4-tap filter on other channels. 
    samplerStateDescr.avs->GainFactor = 44;
    samplerStateDescr.avs->GlobalNoiseEstm = 255;
    samplerStateDescr.avs->StrongEdgeThr = 8;
    samplerStateDescr.avs->WeakEdgeThr = 1;
    samplerStateDescr.avs->StrongEdgeWght = 7;
    samplerStateDescr.avs->RegularWght = 2;
    samplerStateDescr.avs->NonEdgeWght = 1;
#if __INTEL_MDF > 500
    samplerStateDescr.avs->wR3xCoefficient = 6;
    samplerStateDescr.avs->wR3cCoefficient = 15;
    samplerStateDescr.avs->wR5xCoefficient = 9;
    samplerStateDescr.avs->wR5cxCoefficient = 8;
    samplerStateDescr.avs->wR5cCoefficient = 3;	
#endif 

    samplerStateDescr.avs->AvsState->BypassXAF = 0;								// Disable X adaptive filtering
    samplerStateDescr.avs->AvsState->BypassYAF = 0;								// Disable Y adaptive filtering
    samplerStateDescr.avs->AvsState->maxDerivative4Pixels = 7;
    samplerStateDescr.avs->AvsState->maxDerivative8Pixels = 20;
    samplerStateDescr.avs->AvsState->transitionArea4Pixels = 4;
    samplerStateDescr.avs->AvsState->transitionArea8Pixels = 5;
    samplerStateDescr.avs->AvsState->DefaultSharpLvl = 255;	

    samplerStateDescr.avs->ShuffleOutputWriteback = 1;

    for ( int i = 0; i < CM_NUM_COEFF_ROWS_SKL; i++ )
    {
        // Y horizontal
        samplerStateDescr.avs->AvsState->Tbl0X[ i ].FilterCoeff_0_0 = Tbl0X[i][0];
        samplerStateDescr.avs->AvsState->Tbl0X[ i ].FilterCoeff_0_1 = Tbl0X[i][1];
        samplerStateDescr.avs->AvsState->Tbl0X[ i ].FilterCoeff_0_2 = Tbl0X[i][2];
        samplerStateDescr.avs->AvsState->Tbl0X[ i ].FilterCoeff_0_3 = Tbl0X[i][3];
        samplerStateDescr.avs->AvsState->Tbl0X[ i ].FilterCoeff_0_4 = Tbl0X[i][4];
        samplerStateDescr.avs->AvsState->Tbl0X[ i ].FilterCoeff_0_5 = Tbl0X[i][5];
        samplerStateDescr.avs->AvsState->Tbl0X[ i ].FilterCoeff_0_6 = Tbl0X[i][6];
        samplerStateDescr.avs->AvsState->Tbl0X[ i ].FilterCoeff_0_7 = Tbl0X[i][7];
        // Y vertical
        samplerStateDescr.avs->AvsState->Tbl0Y[ i ].FilterCoeff_0_0 = Tbl0Y[i][0];
        samplerStateDescr.avs->AvsState->Tbl0Y[ i ].FilterCoeff_0_1 = Tbl0Y[i][1];
        samplerStateDescr.avs->AvsState->Tbl0Y[ i ].FilterCoeff_0_2 = Tbl0Y[i][2];
        samplerStateDescr.avs->AvsState->Tbl0Y[ i ].FilterCoeff_0_3 = Tbl0Y[i][3];
        samplerStateDescr.avs->AvsState->Tbl0Y[ i ].FilterCoeff_0_4 = Tbl0Y[i][4];
        samplerStateDescr.avs->AvsState->Tbl0Y[ i ].FilterCoeff_0_5 = Tbl0Y[i][5];
        samplerStateDescr.avs->AvsState->Tbl0Y[ i ].FilterCoeff_0_6 = Tbl0Y[i][6];
        samplerStateDescr.avs->AvsState->Tbl0Y[ i ].FilterCoeff_0_7 = Tbl0Y[i][7];
        // UV horizontal
        samplerStateDescr.avs->AvsState->Tbl1X[ i ].FilterCoeff_0_0 = Tbl1X[i][0];
        samplerStateDescr.avs->AvsState->Tbl1X[ i ].FilterCoeff_0_1 = Tbl1X[i][1];
        samplerStateDescr.avs->AvsState->Tbl1X[ i ].FilterCoeff_0_2 = Tbl1X[i][2];
        samplerStateDescr.avs->AvsState->Tbl1X[ i ].FilterCoeff_0_3 = Tbl1X[i][3];
        samplerStateDescr.avs->AvsState->Tbl1X[ i ].FilterCoeff_0_4 = Tbl1X[i][4];
        samplerStateDescr.avs->AvsState->Tbl1X[ i ].FilterCoeff_0_5 = Tbl1X[i][5];
        samplerStateDescr.avs->AvsState->Tbl1X[ i ].FilterCoeff_0_6 = Tbl1X[i][6];
        samplerStateDescr.avs->AvsState->Tbl1X[ i ].FilterCoeff_0_7 = Tbl1X[i][7];
        // UV vertical
        samplerStateDescr.avs->AvsState->Tbl1Y[ i ].FilterCoeff_0_0 = Tbl1Y[i][0];
        samplerStateDescr.avs->AvsState->Tbl1Y[ i ].FilterCoeff_0_1 = Tbl1Y[i][1];
        samplerStateDescr.avs->AvsState->Tbl1Y[ i ].FilterCoeff_0_2 = Tbl1Y[i][2];
        samplerStateDescr.avs->AvsState->Tbl1Y[ i ].FilterCoeff_0_3 = Tbl1Y[i][3];
        samplerStateDescr.avs->AvsState->Tbl1Y[ i ].FilterCoeff_0_4 = Tbl1Y[i][4];
        samplerStateDescr.avs->AvsState->Tbl1Y[ i ].FilterCoeff_0_5 = Tbl1Y[i][5];
        samplerStateDescr.avs->AvsState->Tbl1Y[ i ].FilterCoeff_0_6 = Tbl1Y[i][6];
        samplerStateDescr.avs->AvsState->Tbl1Y[ i ].FilterCoeff_0_7 = Tbl1Y[i][7];
    }
	
    // Create sampler state using sampler state descrepter 

    result = pCmDev->CreateSampler8x8(samplerStateDescr, *pSampler8x8);
    if (result != CM_SUCCESS ) {
        printf("CM CreateSampler8x8 error, result=%d\r\n", result);
        return -1;
    }
    
    return result;
}
