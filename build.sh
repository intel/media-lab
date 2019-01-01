#!/bin/bash

va=video_analytics_Intel_GPU
vaDir=$PWD/$va

cd $vaDir || exit
echo 'Start to build the tool "video_analytics_example"'
source env.sh
[ ! -d build ] && { mkdir build || exit;}
cd build || exit
cmake ../
make -j4 && \
echo -e "\e[32;1mBuild Successfully, you can get the tool \"video_analytics_example\" in $PWD/video_analytics_example/\e[0m" || \
echo -e "\e[31;1mBuild Failed!!!\e[0m"

