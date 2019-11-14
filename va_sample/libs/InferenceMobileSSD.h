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

#ifndef __INFERRENCE_MOBILESSD_H__
#define __INFERRENCE_MOBILESSD_H__

#include "InferenceOV.h"

class InferenceMobileSSD : public InferenceOV
{
public:
    InferenceMobileSSD();
    virtual ~InferenceMobileSSD();

    virtual int Load(const char *device, const char *model, const char *weights);

    virtual void GetRequirements(uint32_t *width, uint32_t *height, uint32_t *fourcc);

protected:
    // derived classes need to fill the dst with the img, based on their own different input dimension
    void CopyImage(const uint8_t *img, void *dst, uint32_t batchIndex);

    // derived classes need to fill VAData by the result, based on their own different output demension
    int Translate(std::vector<VAData *> &datas, uint32_t count, void *result, uint32_t *channels, uint32_t *frames, uint32_t *roiIds);

    int SetDataPorts();

    // model related
    uint32_t m_inputWidth;
    uint32_t m_inputHeight;
    uint32_t m_channelNum;
    uint32_t m_resultSize; // size per one result
    uint32_t m_maxResultNum; // result number per one request
};

#endif //__INFERRENCE_MOBILESSD_H__