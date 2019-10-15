#include "Inference.h"
#include "InferenceMobileSSD.h"
#include "InferenceResnet50.h"

InferenceBlock* InferenceBlock::Create(InferenceModelType type)
{
    switch (type)
    {
        case MOBILENET_SSD_U8:
            return new InferenceMobileSSD;
        case RESNET_50:
            return new InferenceResnet50;
        default:
            return nullptr;
    }
}

void InferenceBlock::Destroy(InferenceBlock *infer)
{
    delete infer;
}


