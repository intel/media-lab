#!/bin/bash -v

rm -rf CMakeCache.txt CMakeFiles Makefile cmake_install.cmake 

cd  common; ./cleanup.sh; cd ..
cd 3rdparty/gflags/; ./cleanup.sh; cd ../..
cd IE_plugin; ./cleanup.sh; cd .. 
cd interop_tutorials/tutorial_1; ./cleanup.sh; cd ../..
cd interop_tutorials/tutorial_2; ./cleanup.sh; cd ../..
cd interop_tutorials/tutorial_3_vmem; ./cleanup.sh; cd ../..
cd media_tutorials/simple_1_session; ./cleanup.sh; cd ../..
cd media_tutorials/simple_2_decode; ./cleanup.sh; cd ../..
cd VAF; ./cleanup.sh; cd ..


