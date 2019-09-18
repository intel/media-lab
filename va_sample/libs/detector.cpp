#include "detector.h"
#include <ie_plugin_config.hpp>
#include <ie_plugin_ptr.hpp>
#include <cpp/ie_cnn_net_reader.h>
#include <inference_engine.hpp>

#include <ext_list.hpp>
#include <ie_plugin_dispatcher.hpp>
#include <ie_plugin_ptr.hpp>
#include <cpp/ie_cnn_net_reader.h>
#include <cpp/ie_infer_request.hpp>
#include <ie_device.hpp>

Detector::Detector() :
    nbatch_index_(0),
    num_channels_(0),
    num_batch_(0),
    maxProposalCount(0),
    objectSize(0),
    bisASync(false),
    bLoad(false)
{
	bLoad = false;
}

int Detector::Load(string& device, const string& model_file,const string& weights_file, int bn) {
	if (bLoad)
		return 0;
	err_msg = "";
        std::cout<<"> start to build inference instance on device:"<<device<<std::endl;
// --------------------Load network (Generated xml/bin files)-------------------------------------------
	//InferenceEngine::InferencePlugin enginePtr;
	try {
		enginePtr = PluginDispatcher({ "" }).getPluginByDevice(device.c_str());
	}
	catch (InferenceEngineException e) {
               std::cout<<"  can not find pluginDevice"<<std::endl;
		return -1;
	}

	/** Loading default extensions **/
	if (device.find("CPU") != std::string::npos) {
		/**
			* cpu_extensions library is compiled from "extension" folder containing
			* custom MKLDNNPlugin layer implementations. These layers are not supported
			* by mkldnn, but they can be useful for inferring custom topologies.
		**/
		enginePtr.AddExtension(std::make_shared<Extensions::Cpu::CpuExtensions>());
	}
/** Read network model **/ 
	InferenceEngine::CNNNetReader netReader;
	try {
		netReader.ReadNetwork(model_file);
		netReader.ReadWeights(weights_file);
	}
	catch (InferenceEngineException e) {
		std::cout<<"  can not load model:"<<model_file<<std::endl;
		return -1;
	}

	network_ = netReader.getNetwork();
	network_.setBatchSize(bn);
        std::cout<<">  set batch size: "<<bn<< std::endl;
	num_batch_ = (int)network_.getBatchSize();  //IE can not support dynamically batch for ssd
// ---------------------------Set inputs ------------------------------------------------------	
	InferenceEngine::InputsDataMap inputInfo(network_.getInputsInfo());
	auto& inputInfoFirst = inputInfo.begin()->second;
#ifdef INPUT_U8
	inputInfoFirst->setPrecision(Precision::U8); //mean and scale move to IE
#else
	inputInfoFirst->setPrecision(Precision::FP32); //since mean and scale, here must set FP32
#endif
	inputInfoFirst->getInputData()->setLayout(Layout::NCHW);  //default is NCHW
	inputname = inputInfo.begin()->first;
// ---------------------------Set outputs ------------------------------------------------------	
	InferenceEngine::OutputsDataMap outputInfo(network_.getOutputsInfo());
	auto& _output = outputInfo.begin()->second;
	_output->setPrecision(Precision::FP32);
	_output->setLayout(Layout::NCHW);	
	outputname = outputInfo.begin()->first;
	const InferenceEngine::SizeVector outputDims = _output->getTensorDesc().getDims();
	maxProposalCount = (int)outputDims[2];
	objectSize = (int)outputDims[3];

        std::cout<<"   maxProposalCount:"<<maxProposalCount<<std::endl;
	if (objectSize != 7) {
		throw std::logic_error("  Output should have 7 as a last dimension");
	}
	if (outputDims.size() != 4) {
		throw std::logic_error("  Incorrect output dimensions for SSD");
	}
// -------------------------Loading model to the plugin-------------------------------------------------
	//InferenceEngine::ExecutableNetwork exenet;
	try {
		exenet = enginePtr.LoadNetwork(network_, {});
	}
	catch (InferenceEngineException e) {
		std::cout<<"   Input Model file"<< model_file<<" doesn't support by current device:"<<device<<std::endl;
		return -1;
	}
	infer_request_curr_ = exenet.CreateInferRequestPtr();
	infer_request_next_ = exenet.CreateInferRequestPtr();
	Blob::Ptr imageInput = infer_request_curr_->GetBlob(inputname);
	num_channels_ = (int)imageInput->dims()[2];

	if(!(num_channels_ == 3 || num_channels_ == 1))
		throw std::logic_error("Input layer should have 1 or 3 channels");
	input_geometry_ = cv::Size((int)imageInput->dims()[0], (int)imageInput->dims()[1]);	
	CreateMean();	
	nbatch_index_ = 0;
	bisASync = false;
	bLoad = true;
        return 0;
}

Detector::~Detector() {
	EmptyQueue(imginfoque_);
}

void Detector::EmptyQueue(queue<Detector::ImageInfo>& que)
{
	while (!que.empty()) {
		que.pop();
	}
}

/* Wrap the input layer of the network in separate cv::Mat objects
* (one per channel). This way we save one memcpy operation */
void Detector::WrapInputLayer(IDtype* input_data) {
	input_channels.clear();
	int width = input_geometry_.width;
	int height = input_geometry_.height;
	for (int i = 0; i < num_batch_*num_channels_; ++i) {
#ifdef INPUT_U8
		cv::Mat channel(height, width, CV_8UC1, input_data);
#else
		cv::Mat channel(height, width, CV_32FC1, input_data);
#endif
		input_channels.push_back(channel);
		input_data += width * height;
	}
}

cv::Mat Detector::PreProcess(const cv::Mat& img) {
	
	cv::Mat sample_float;
	if (num_channels_ == 3)
		img.convertTo(sample_float, CV_32FC3);
	else
		img.convertTo(sample_float, CV_32FC1);
	
	/* Convert the input image to the input image format of the network. */
	cv::scaleAdd (sample_float, 0.007843, mean_, sample_float); //scaleAdd or (add+multiply)? which speed : ans is scaleAdd
	
	return sample_float;

}

void Detector::CreateMean() {
#ifndef INPUT_U8
	if (num_channels_ == 3)
		mean_= cv::Mat(input_geometry_, CV_32FC3, cv::Scalar(-127.5*0.007843,-127.5*0.007843,-127.5*0.007843));
	else
		mean_= cv::Mat(input_geometry_, CV_32FC1, cv::Scalar(-127.5*0.007843));	
#endif
}

/*
	InsertImage will fill a blob until blob full, if full return the blob point
*/
Detector::InsertImgStatus Detector::InsertImage(const cv::Mat& orgimg, vector<DetctorResult>& objects, int inputid, int frameno, int channelid) {
	InsertImgStatus retvalue=INSERTIMG_INSERTED;
	if(orgimg.cols==0 || orgimg.rows==0 ||!bLoad)
		return INSERTIMG_NULL;
	if(nbatch_index_==0){  //Wrap a blob
		WrapInputLayer(static_cast<IDtype*>((bisASync ? infer_request_next_ : infer_request_curr_)->GetBlob(inputname)->buffer()));
	}
	ImageInfo is;
	is.isize = orgimg.size();
	is.inputid = inputid;
	is.orgimg = orgimg;
    is.frameno = frameno;
    is.channelid = channelid;
	imginfoque_.push(is);
        // No need to process it due to VPP output is already scaled.
	//cv::Mat img = PreProcess(orgimg);		
	//cv::split(img, &input_channels[num_channels_*nbatch_index_]);
#ifdef INPUT_U8
        cv::split(orgimg, &input_channels[num_channels_*nbatch_index_]);
#else
	cv::Mat img = PreProcess(orgimg);		
	cv::split(img, &input_channels[num_channels_*nbatch_index_]);

#endif
	//if get return INSERTIMG_GET
	if(++nbatch_index_>=num_batch_){
		nbatch_index_=0;
		(bisASync ? infer_request_next_ : infer_request_curr_)->StartAsync();
		if (InferenceEngine::OK == infer_request_curr_->Wait(IInferRequest::WaitMode::RESULT_READY)) {
			/* get the result */
			//objects.clear();
			objects.resize(num_batch_);
			for (int i = 0; i<num_batch_; i++) {
				if (!imginfoque_.empty()) {
					objects[i].imgsize = imginfoque_.front().isize;
					objects[i].inputid = imginfoque_.front().inputid;
					objects[i].orgimg  = imginfoque_.front().orgimg;
                    objects[i].frameno = imginfoque_.front().frameno;
				    objects[i].channelid = imginfoque_.front().channelid;
					imginfoque_.pop();
					objects[i].boxs.clear();
				}
				else
					throw std::logic_error("InsertImage queue fail");
			}
			retvalue = INSERTIMG_PROCESSED;

                        const float *result = infer_request_curr_->GetBlob(outputname)->buffer().as<PrecisionTrait<Precision::FP32>::value_type*>();
			for (int k = 0; k < maxProposalCount; k++) {
				resultbox object;
				int imgid = (int)result[0];
				if (imgid < 0 || result[2] == 0 || imgid > num_batch_) {
					result += objectSize;
					break;
				}
				int w = objects[imgid].imgsize.width;
				int h = objects[imgid].imgsize.height;
				object.classid = (int)result[1];
				object.confidence = result[2];
				object.left = (int)(result[3] * w);
				object.top = (int)(result[4] * h);
				object.right = (int)(result[5] * w);
				object.bottom = (int)(result[6] * h);
				if (object.left < 0) object.left = 0;
				if (object.top < 0) object.top = 0;
				if (object.right >= w) object.right = w - 1;
				if (object.bottom >= h) object.bottom = h - 1;
				objects[imgid].boxs.push_back(object);
				result += objectSize;
				retvalue = INSERTIMG_GET;
			}
		}
		if(bisASync)
			infer_request_curr_.swap(infer_request_next_);
	}
	return retvalue;
}

void Detector::SetMode(bool isSync)
{
	infer_request_curr_->Wait(IInferRequest::WaitMode::RESULT_READY);
	infer_request_next_->Wait(IInferRequest::WaitMode::RESULT_READY);
	EmptyQueue(imginfoque_);
	nbatch_index_ = 0;
	bisASync = isSync;
}
