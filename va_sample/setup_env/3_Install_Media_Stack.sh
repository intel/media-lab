#!/bin/sh

WORKSPACE=$PWD

mkdir media_stack

# install gmmlib
cd $WORKSPACE/media_stack
git clone https://github.com/intel/gmmlib/
cd gmmlib
git reset --hard intel-gmmlib-19.3.2
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=/usr/lib/x86_64-linux-gnu
make -j8
sudo make install

# install libva
cd $WORKSPACE/media_stack
git clone https://github.com/intel/libva
cd libva
git reset --hard 2.6.0.pre1
./autogen.sh --prefix=/usr --libdir=/usr/lib/x86_64-linux-gnu
make -j8
sudo make install

# install media driver
cd $WORKSPACE/media_stack
git clone https://github.com/intel/media-driver
cd media-driver
git reset --hard intel-media-19.3.1
cd ..
mkdir build
cd build
cmake ../media-driver/ -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=/usr/lib/x86_64-linux-gnu
make -j8
sudo make install

# install MediaSDK
cd $WORKSPACE/media_stack
git clone https://github.com/Intel-Media-SDK/MediaSDK; cd MediaSDK; git reset --hard intel-mediasdk-19.3.1
mkdir build
cd build
cmake ../
make -j8
sudo make install

