#ifndef __INFERRENCE_MOBILESSD_H__
#define __INFERRENCE_MOBILESSD_H__

#include "InferenceOV.h"

class InferenceMobileSSD : public InferenceOV
{
public:
    InferenceMobileSSD();
    virtual ~InferenceMobileSSD();

    virtual int Load(const char *device, const char *model, const char *weights);

protected:
    // derived classes need to fill the dst with the img, based on their own different input dimension
    void CopyImage(const cv::Mat &img, void *dst, uint32_t batchIndex);

    // derived classes need to fill VAData by the result, based on their own different output demension
    int Translate(std::vector<VAData *> &datas, uint32_t count, void *result, uint32_t *channels, uint32_t *frames);

    // model related
    uint32_t m_inputWidth;
    uint32_t m_inputHeight;
    uint32_t m_channelNum;
    uint32_t m_resultSize; // size per one result
    uint32_t m_maxResultNum; // result number per one request
};

#endif //__INFERRENCE_MOBILESSD_H__