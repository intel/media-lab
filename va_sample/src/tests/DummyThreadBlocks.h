/*
// Copyright (c) 2019 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/
#include "ThreadBlock.h"
#include <mfxvideo++.h>
#include <stdio.h>
#include <unistd.h>

class DummyDecodeThread : public VAThreadBlock
{
public:
    DummyDecodeThread(uint32_t channel):
        m_channel(channel),
        m_decodeRefNum(1),
        m_vpRefNum(1),
        m_vpRatio(1)
    {
    }

    void SetDecodeOutputRef(int ref) {m_decodeRefNum = ref; }

    void SetVPOutputRef(int ref) {m_vpRefNum = ref;}

    void SetVPRatio(uint32_t ratio) {m_vpRatio = ratio; }

    int Loop()
    {
        uint32_t frameIndex = 0;
        while (true)
        {
            usleep(33000);
        
            VAData *decodeData = VAData::Create(nullptr, 1920, 1080, 1920, MFX_FOURCC_NV12);
            decodeData->SetID(m_channel, frameIndex);
            decodeData->SetRef(m_decodeRefNum);
            VADataPacket *packet = DequeueOutput();
            packet->push_back(decodeData);
            printf("Decode %d: Ouput decode frame %d\n", m_channel, frameIndex);

            if (frameIndex % m_vpRatio == 0)
            {
                VAData *vpData = VAData::Create(nullptr, 330, 330, 330, MFX_FOURCC_RGBP);
                vpData->SetID(m_channel, frameIndex);
                vpData->SetRef(m_vpRefNum);
                packet->push_back(vpData);
                printf("Decode %d: Output vp frame %d\n", m_channel, frameIndex);
            }
            ++ frameIndex;
            EnqueueOutput(packet);
        }
    }

    
protected:
    uint32_t m_decodeRefNum;
    uint32_t m_vpRefNum;
    uint32_t m_channel;
    uint32_t m_vpRatio;
};

class DummyInferenceThread : public VAThreadBlock
{
public:
    DummyInferenceThread(uint32_t index):
        m_index(index)
    {
    }

    int Loop()
    {
        while (true)
        {
            usleep(100000);

            VADataPacket *InPacket = AcquireInput();
            VADataPacket *OutPacket = DequeueOutput();

            VAData *vpOut = nullptr;

            for (auto ite = InPacket->begin(); ite != InPacket->end(); ite ++)
            {
                VAData *data = *ite;
                if (CanBeProcessed(data))
                {
                    vpOut = data;
                }
                else
                {
                    OutPacket->push_back(data);
                }
            }

            ReleaseInput(InPacket);

            if (vpOut)
            {
                printf("Inference %d: Processing channel %d, frame %d\n", m_index, vpOut->ChannelIndex(), vpOut->FrameIndex());
                VAData *outputData = VAData::Create(0, 0, 10, 10);
                outputData->SetID(vpOut->ChannelIndex(), vpOut->FrameIndex());
                vpOut->DeRef(OutPacket);
                OutPacket->push_back(outputData);
            }
            EnqueueOutput(OutPacket);
        }
    }

protected:
    bool CanBeProcessed(VAData *data)
    {
        uint32_t w, h, p, fourcc;
        data->GetSurfaceInfo(&w, &h, &p, &fourcc);
        return (w == 330) && (h == 330) && (fourcc == MFX_FOURCC_RGBP);
    }

    uint32_t m_index;
};


class DummyTrackingThread : public VAThreadBlock
{
public:
    DummyTrackingThread(uint32_t index):
        m_index(index)
    {
    }

    int Loop()
    {
        while (true)
        {
            usleep(1000);

            VADataPacket *InPacket = AcquireInput();
            VADataPacket *OutPacket = DequeueOutput();

            VAData *roi = nullptr;
            VAData *decodeOutput = nullptr;

            for (auto ite = InPacket->begin(); ite != InPacket->end(); ite ++)
            {
                VAData *data = *ite;
                if (IsROI(data))
                {
                    roi = data;
                }
                else if (IsDecodeOutput(data))
                {
                    decodeOutput = data;
                }
                else
                {
                    OutPacket->push_back(data);
                }
            }
            ReleaseInput(InPacket);

            if (decodeOutput == nullptr)
            {
                printf("Error: tracking thread doesn't get a decoded frame\n");
            }
            else if (roi == nullptr)
            {
                printf("Tracker %d: Create a new ROI, channel %d, frame %d\n", m_index,
                                                                               decodeOutput->ChannelIndex(),
                                                                               decodeOutput->FrameIndex());
                VAData *outputData = VAData::Create(0, 0, 20, 20);
                outputData->SetID(decodeOutput->ChannelIndex(), decodeOutput->FrameIndex());
                decodeOutput->DeRef(OutPacket);
                OutPacket->push_back(outputData);
            }
            else
            {
                printf("Tracker %d: Copy inference ROI, channel %d, frame %d\n", m_index,
                                                                               decodeOutput->ChannelIndex(),
                                                                               decodeOutput->FrameIndex());
                decodeOutput->DeRef(OutPacket);
                uint32_t x, y, w, h;
                roi->GetRoiRegion(&x, &y, &w, &h);
                VAData *outputData = VAData::Create(x, y, w, h);
                outputData->SetID(roi->ChannelIndex(), roi->FrameIndex());
                roi->DeRef(OutPacket);
                OutPacket->push_back(outputData);
            }

            EnqueueOutput(OutPacket);
        }
    }

protected:
    bool IsROI(VAData *data)
    {
        return data->Type() == ROI_REGION;
    }

    bool IsDecodeOutput(VAData *data)
    {
        uint32_t w, h, p, fourcc;
        data->GetSurfaceInfo(&w, &h, &p, &fourcc);
        return (w == 1920 && h == 1080 && fourcc == MFX_FOURCC_NV12);
    }

    
    uint32_t m_index;
};

class DummyDisplayThread : public VAThreadBlock
{
public:
    DummyDisplayThread()
    {
    }

    int Loop()
    {
        while (true)
        {
            usleep(16000);

            VADataPacket *InPacket = AcquireInput();
            VADataPacket *OutPacket = DequeueOutput();

            VAData *roi = nullptr;
            VAData *decodeOutput = nullptr;

            for (auto ite = InPacket->begin(); ite != InPacket->end(); ite ++)
            {
                VAData *data = *ite;
                if (IsROI(data))
                {
                    roi = data;
                }
                else if (IsDecodeOutput(data))
                {
                    decodeOutput = data;
                }
                else
                {
                    OutPacket->push_back(data);
                }
            }
            ReleaseInput(InPacket);

            if (decodeOutput == nullptr)
            {
                printf("Error: display thread doesn't get a decoded frame\n");
            }
            else if (roi == nullptr)
            {
                printf("Error: display thread doesn't get a roi region frame\n");
            }
            else
            {
                printf("Display: channel %d, frame %d\n", decodeOutput->ChannelIndex(), decodeOutput->FrameIndex());
                decodeOutput->DeRef(OutPacket);
                roi->DeRef(OutPacket);
            }

            EnqueueOutput(OutPacket);
        }
    }

protected:
    bool IsROI(VAData *data)
    {
        return data->Type() == ROI_REGION;
    }

    bool IsDecodeOutput(VAData *data)
    {
        uint32_t w, h, p, fourcc;
        data->GetSurfaceInfo(&w, &h, &p, &fourcc);
        return (w == 1920 && h == 1080 && fourcc == MFX_FOURCC_NV12);
    }
};
