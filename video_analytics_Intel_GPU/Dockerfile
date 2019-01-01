FROM ubuntu:16.04
MAINTAINER monique.jones@intel.com
LABEL Name=tutorials-base-test Version=0.1.0

RUN apt-get update \
    && apt install -y software-properties-common cpio \
    && rm -rf /var/lib/apt/lists/*

RUN add-apt-repository -y ppa:teejee2008/ppa
RUN apt-get update && apt-get install -y \
        git \
        libssl-dev \
        dh-autoreconf \
        cmake \
        libgl1-mesa-dev \
        libpciaccess-dev \
        build-essential \
        curl \
        kmod \
        imagemagick \
        gedit \
        mplayer \
        unzip \
        yasm \
        libjpeg-dev \
        linux-firmware \
	openssl \
	libnuma1 \
	libpciaccess0 \
	xz-utils \
	bc \
	curl \
	libssl-dev\
	wget \
	clinfo \
	libomp-dev \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && apt-get install -y \
        libopencv-dev \
        checkinstall \
        pkg-config \
        libgflags-dev \
        udev \
        libxcb-xv0-dev \
	sudo \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /vait
COPY 3rdparty /vait/3rdparty

COPY silent.cfg /vait/silent.cfg
RUN mkdir /tmp/vait && cp silent.cfg /tmp/vait/


COPY install.py /tmp/vait/install.py
COPY env.sh /vait/env.sh
COPY kcfGPU /vait/kcfGPU
RUN cd /tmp/vait && python install.py
RUN rm -r /tmp/vait

ENV MFX_HOME=/opt/intel/mediasdk
ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/lib/x86_64-linux-gnu:/usr/lib
ENV LIBVA_DRIVERS_PATH=/usr/lib/x86_64-linux-gnu/dri
ENV LIBVA_DRIVER_NAME=iHD
ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/intel/tbb/lib

RUN cd /vait/

COPY cmake      /vait/cmake
COPY common      /vait/common
COPY test_content /vait/test_content
COPY extension /vait/extension
COPY runtime   /vait/runtime
COPY video_analytics_example /vait/video_analytics_example
COPY CMakeLists.txt /vait/CMakeLists.txt
RUN /bin/bash -c "source /vait/env.sh && echo ${LD_LIBRARY_PATH} && export LIBRARY_PATH=${LD_LIBRARY_PATH}:${LIBRARY_PATH} && echo ${LIBRARY_PATH} && mkdir build && cd build && cmake .. && make -j 12"

WORKDIR /vait/build/video_analytics_example
CMD /bin/bash -c "source /vait/env.sh && ./video_analytics_example -d GPU -c 1 -batch 6"
