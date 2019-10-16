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
    int Prepare();

    int Loop();

protected:
    bool CanBeProcessed(VAData *data);

    uint32_t m_index;
    InferenceModelType m_type;
    uint32_t m_asyncDepth;
    uint32_t m_batchNum;
    const char *m_device;
    const char *m_modelFile;
    const char *m_weightsFile;
    InferenceBlock *m_infer;
};