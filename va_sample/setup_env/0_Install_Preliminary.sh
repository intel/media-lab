#!/bin/sh

# install packages
sudo apt install -y git
sudo apt install -y make gcc g++
sudo apt install -y autoconf libtool libdrm-dev xorg xorg-dev openbox libx11-dev libgl1-mesa-glx libgl1-mesa-dev

# get cmake 3.15 package and build and install
wget https://github.com/Kitware/CMake/releases/download/v3.15.5/cmake-3.15.5.tar.gz
tar xvf cmake-3.15.5.tar.gz
cd cmake-3.15.5
./configure --prefix=/usr --bindir=/bin
make -j8
sudo make install
