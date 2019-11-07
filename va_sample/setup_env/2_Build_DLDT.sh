#!/bin/sh

WORKSPACE=$PWD

git clone https://github.com/opencv/dldt
cd dldt/inference-engine
git reset --hard 2019_R3.1
git submodule init
git submodule update --recursive
chmod +x install_dependencies.sh
./install_dependencies.sh
cd $WORKSPACE/dldt/inference-engine
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j8
