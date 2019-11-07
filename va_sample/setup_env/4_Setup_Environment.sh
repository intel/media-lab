#!/bin/sh

WORKSPACE=$PWD

echo export LIBVA_DRIVERS_PATH=/usr/lib/x86_64-linux-gnu/dri >> ~/.bashrc
echo export LIBVA_DRIVER_NAME=iHD >> ~/.bashrc
echo export MFX_HOME=/opt/intel/mediasdk >> ~/.bashrc
echo export DLDT_DIR=$WORKSPACE/dldt >> ~/.bashrc
echo export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$WORKSPACE/dldt/inference-engine/temp/opencv_4.1.2_ubuntu18/lib:$WORKSPACE/dldt/inference-engine/temp/tbb/lib:$WORKSPACE/dldt/inference-engine/bin/intel64/Release/lib >> ~/.bashrc

. ~/.bashrc

