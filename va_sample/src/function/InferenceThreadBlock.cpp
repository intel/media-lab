#include "InferenceThreadBlock.h"

InferenceThreadBlock::InferenceThreadBlock(int channel)
{

}

InferenceThreadBlock::~InferenceThreadBlock()
{
    
}

int InferenceThreadBlock::Prepare()
{
    int ret = 0;
    
    ret = d.Load(device, model_xml, model_bin, 1);
    if (ret)
    {
        std::cout << "load model failed!\n";
        return -1;
    }

    cv::Size net_size = d.GetNetSize();
    int input_width = net_size.width;
    int input_height = net_size.height;

    return 0;
}

int InferenceThreadBlock::Loop()
{
    while (true)
    {
        usleep(100000);

        VADataPacket *InPacket = AcquireInput();
        VADataPacket *OutPacket = DequeueOutput();

        VAData *vpOut = nullptr;

        for (auto ite = InPacket->begin(); ite != InPacket->end(); ite++)
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

            cv::Mat frame;
            Detector::InsertImgStatus status;
            vector<Detector::DetctorResult> objects;

            uint32_t w, h, p, fourcc;
            vpOut->GetSurfaceInfo(&w, &h, &p, &fourcc);
            uint8_t *data = vpOut->GetSurfacePointer();
            std::vector<cv::Mat> channels;
            cv::Mat R = cv::Mat(w, h, CV_8UC1, data);
            cv::Mat G = cv::Mat(w, h, CV_8UC1, data+w*h);
            cv::Mat B = cv::Mat(w, h, CV_8UC1, data+w*h*2);
            channels.push_back(R);
            channels.push_back(G);
            channels.push_back(B);
            cv::merge(channels, frame);

            status = d.InsertImage(frame, objects, 0, 0);

            if (Detector::INSERTIMG_GET == status || Detector::INSERTIMG_PROCESSED == status)
            {
                std::cout << " Infer one frame : objects = " << objects.size() << std::endl;

                for (int k = 0; k < objects.size(); k++)
                {
                    std::cout << " detect result: " << objects[k].channelid << " boxes = " << objects[k].boxs.size() << std::endl;
                    for (auto b : objects[k].boxs)
                    {
                        std::cout << "ClassID = " << b.classid << ", Confidence = " << b.confidence << ", [" << b.left << ", " << b.top << ", " << b.right << ", " << b.bottom << "]\n";
                    }
                }
            }

            VAData *outputData = VAData::Create(0, 0, 10, 10);
            outputData->SetID(vpOut->ChannelIndex(), vpOut->FrameIndex());
            vpOut->DeRef(OutPacket);
            OutPacket->push_back(outputData);
        }
        EnqueueOutput(OutPacket);
    }
    return 0;
}