#!/bin/bash

va=video_analytics_Intel_GPU
vaDir=$PWD/$va
tempDir=$PWD/temp

cd $vaDir || exit
echo "Start to run \"./prerequisites_install_ubuntu.py\" to install the prerequisites"
[ -d $tempDir ] && { rm -rf $tempDir || exit; }
mkdir $tempDir
cp prerequisites_install_ubuntu.py silent.cfg $tempDir
cd $tempDir || exit
sudo ./prerequisites_install_ubuntu.py
echo "pre-build DONE! Please run build.sh to continue"
