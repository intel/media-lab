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

#ifndef __INFERRENCE_H__
#define __INFERRENCE_H__
#pragma warning(disable:4251)  //needs to have dll-interface to be used by clients of class 

#include <stdint.h>
#include <vector>

class VAData;

enum InferenceModelType
{
    MOBILENET_SSD_U8,
    RESNET_50
};

class InferenceBlock
{
public:
    static InferenceBlock *Create(InferenceModelType type);

    static void Destroy(InferenceBlock *infer);

    virtual int Initialize(uint32_t batch_num = 1, uint32_t async_depth = 1) = 0;

    // derived classes need to get input dimension and output dimension besides the base Load operation
    virtual int Load(const char *device, const char *model, const char *weights) = 0;

    // the img should already be in format that the model requests, otherwise, do the conversion outside
    virtual int InsertImage(const uint8_t *img, uint32_t channelId, uint32_t frameId, uint32_t roiId = 0) = 0;

    virtual int Wait() = 0;

    virtual int GetOutput(std::vector<VAData *> &datas, std::vector<uint32_t> &channels, std::vector<uint32_t> &frames) = 0;

    virtual void GetRequirements(uint32_t *width, uint32_t *height, uint32_t *fourcc) = 0;

protected:
    InferenceBlock() {};
    virtual ~InferenceBlock() {};
};

#endif //__INFERRENCE_H__