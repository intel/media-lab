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
    virtual int InsertImage(const uint8_t *img, uint32_t channelId, uint32_t frameId) = 0;

    virtual int Wait() = 0;

    virtual int GetOutput(std::vector<VAData *> &datas, std::vector<uint32_t> &channels, std::vector<uint32_t> &frames) = 0;

    virtual void GetRequirements(uint32_t *width, uint32_t *height, uint32_t *fourcc) = 0;

protected:
    InferenceBlock() {};
    virtual ~InferenceBlock() {};
};

#endif //__INFERRENCE_H__