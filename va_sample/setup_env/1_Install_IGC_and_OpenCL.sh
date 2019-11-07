#!/bin/sh

WORKSPACE=$PWD

mkdir neo
cd neo

wget https://github.com/intel/compute-runtime/releases/download/19.43.14583/intel-gmmlib_19.3.2_amd64.deb
wget https://github.com/intel/compute-runtime/releases/download/19.43.14583/intel-igc-core_1.0.2714.1_amd64.deb
wget https://github.com/intel/compute-runtime/releases/download/19.43.14583/intel-igc-opencl_1.0.2714.1_amd64.deb
wget https://github.com/intel/compute-runtime/releases/download/19.43.14583/intel-opencl_19.43.14583_amd64.deb
wget https://github.com/intel/compute-runtime/releases/download/19.43.14583/intel-ocloc_19.43.14583_amd64.deb
sudo dpkg -i *.deb

cd $WORKSPACE
