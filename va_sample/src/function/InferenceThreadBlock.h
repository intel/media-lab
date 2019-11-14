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

#pragma once

#include <stdio.h>
#include <unistd.h>
#include <string>

#include "ThreadBlock.h"
#include "../../libs/Inference.h"

class InferenceBlock;

class InferenceThreadBlock : public VAThreadBlock
{
public:
    InferenceThreadBlock(uint32_t index, InferenceModelType type);

    ~InferenceThreadBlock();

    inline void SetAsyncDepth(uint32_t depth) {m_asyncDepth = depth; }
    inline void SetBatchNum(uint32_t batch) {m_batchNum = batch; }
    inline void SetDevice(const char *device) {m_device = device; }
    inline void SetModelFile(const char *model, const char *weights)
    {
        m_modelFile = model;
        m_weightsFile = weights;
    }
    inline void SetOutputRef(int ref) {m_outRef = ref; }

    int Loop();

protected:
    int PrepareInternal() override;

    bool CanBeProcessed(VAData *data);

    uint32_t m_index;
    InferenceModelType m_type;
    uint32_t m_asyncDepth;
    uint32_t m_batchNum;
    int m_outRef;
    const char *m_device;
    const char *m_modelFile;
    const char *m_weightsFile;
    InferenceBlock *m_infer;
};