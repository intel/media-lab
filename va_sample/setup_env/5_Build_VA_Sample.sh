#!/bin/sh

WORKSPACE=$PWD

git clone -b va_sample --single-branch https://github.com/intel/media-lab
cd media-lab/va_sample
mkdir build
cd build
cmake ..
make -j8
