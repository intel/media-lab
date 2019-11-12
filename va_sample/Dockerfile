FROM ubuntu:18.04
MAINTAINER fan.f.he@intel.com
LABEL Name=va-sample-test Version=0.1.0

RUN apt-get update
RUN apt-get install -y wget
WORKDIR /workspace

COPY setup_env/* /workspace/

RUN sed -i 's/sudo//g' 0_Install_Preliminary.sh
RUN sed -i 's/sudo//g' 1_Install_IGC_and_OpenCL.sh
RUN sed -i 's/sudo//g' 2_Build_DLDT.sh
RUN sed -i 's/sudo//g' 3_Install_Media_Stack.sh
RUN sed -i 's/sudo//g' 4_Setup_Environment.sh
RUN sed -i 's/sudo//g' 5_Build_VA_Sample.sh

RUN ./0_Install_Preliminary.sh
#RUN git config --global http.proxy http://host:port
#RUN git config --global https.proxy http://host:port
RUN ./1_Install_IGC_and_OpenCL.sh
RUN apt-get install -y libusb-1.0-0-dev
RUN apt-get install -y python2.7 python-pip
RUN ./2_Build_DLDT.sh
RUN ./3_Install_Media_Stack.sh
#RUN ./4_Setup_Environment.sh
ENV LIBVA_DRIVERS_PATH=/usr/lib/x86_64-linux-gnu/dri
ENV LIBVA_DRIVER_NAME=iHD
ENV MFX_HOME=/opt/intel/mediasdk
ENV DLDT_DIR=/workspace/dldt
ENV LD_LIBRARY_PATH=/workspace/dldt/inference-engine/temp/opencv_4.1.2_ubuntu18/lib:/workspace/dldt/inference-engine/temp/tbb/lib:/workspace/dldt/inference-engine/bin/intel64/Release/lib
RUN ./5_Build_VA_Sample.sh

COPY 2m.264 /workspace/
WORKDIR /workspace/media-lab/va_sample/build/bin
CMD /bin/bash -c "./SamplePipeline -i /workspace/2m.264"
