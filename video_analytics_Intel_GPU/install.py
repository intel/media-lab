#!/usr/bin/python
"""

Copyright (c) 2018 Intel Corporation All Rights Reserved.

THESE MATERIALS ARE PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR ITS
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THESE
MATERIALS, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: VAinstaller.py

Abstract: Complete install for Intel Visual Analytics, including
 * Intel(R) Media SDK
 * Intel(R) Computer Vision SDK or Intel(R) Deep Learning Deployment Toolkit
 * Drivers
 * Prerequisites
"""

import os, sys, platform
import os.path
import argparse
import grp

class diagnostic_colors:
    ERROR   = '\x1b[31;1m'  # Red/bold
    SUCCESS = '\x1b[32;1m'  # green/bold
    RESET   = '\x1b[0m'     # Reset attributes
    INFO    = '\x1b[34;1m'  # info
    OUTPUT  = ''            # command's coutput printing
    STDERR  = '\x1b[36;1m'  # cyan/bold
    SKIPPED = '\x1b[33;1m'  # yellow/bold

class loglevelcode:
    ERROR   = 0
    SUCCESS = 1
    INFO    = 2

GLOBAL_LOGLEVEL=3

def print_info( msg, loglevel ):
    global GLOBAL_LOGLEVEL

    """ printing information """    
    
    if loglevel==loglevelcode.ERROR and GLOBAL_LOGLEVEL>=0:
        color = diagnostic_colors.ERROR
        msgtype=" [ ERROR ] "
        print( color + msgtype + diagnostic_colors.RESET + msg )
    elif loglevel==loglevelcode.SUCCESS and GLOBAL_LOGLEVEL>=1:
        color = diagnostic_colors.SUCCESS
        msgtype=" [ OK ] "
        print( color + msgtype + diagnostic_colors.RESET + msg )
    elif loglevel==loglevelcode.INFO and GLOBAL_LOGLEVEL>=2:
        color = diagnostic_colors.INFO
        msgtype=" [ INFO ] "
        print( color + msgtype + diagnostic_colors.RESET + msg )


    
    return

def run_cmd(cmd):
    output=""
    fin=os.popen(cmd+" 2>&1","r")
    for line in fin:
        output+=line
    fin.close()
    return output

def find_library(libfile):
    search_path=os.environ.get("LD_LIBRARY_PATH","/usr/lib64")
    if not ('/usr/lib64' in search_path):
	search_path+=";/usr/lib64"
    paths=search_path.split(";")

    found=False
    for libpath in paths:
        if os.path.exists(libpath+"/"+libfile):
            found=True
            break
    
    return found

def fnParseCommandline():
    if len(sys.argv) == 1:		
        return "-all"
    elif len(sys.argv) > 3:
        return "not support"
        exit(0)
	
    if sys.argv[1] == "-h":
        print "[%s" % sys.argv[0] + " usage]"
        print "\t -h: display help"
        print "\t -all: install all components"
        print "\t -b BUILT_TARGET"
        print "\t -nomsdk: skip prerequisites for MSDK"
        print "\t -cvsdk: install OpenVINO"
        exit(0)

    return sys.argv[1]

if __name__ == "__main__":

    if os.getuid() != 0:
        exit("Must be run as root. Exiting.")

    WORKING_DIR=run_cmd("pwd").strip()
    msg_tmp = "Working directory: " + WORKING_DIR
    print_info(msg_tmp, loglevelcode.INFO)

    BUILD_TARGET=""
    enable_msdk = True
    install_cvsdk = False

    cmd = fnParseCommandline()

    if cmd == "-b":
        BUILD_TARGET=sys.argv[2]
        print "BUILD_TARGET=", BUILD_TARGET
    elif cmd == "-nomsdk":
        print_info("Skip prerequisites installation for MediaSDK interop tutorials", loglevelcode.INFO)
        enable_msdk = False
    elif cmd == "-cvsdk":
        print_info("Install legacy CVSDK R3 package", loglevelcode.INFO)
        install_cvsdk = True

    print ""
    print "************************************************************************"
    print_info("Install required tools and create build environment.", loglevelcode.INFO)
    print "************************************************************************"
   

    if enable_msdk == True:
        # Firmware update  
        cmd = "git clone https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git"
        os.system(cmd);

        if BUILD_TARGET == "BXT":
            cmd = "cp linux-firmware/i915/bxt_dmc_ver1_07.bin /lib/firmware/i915/.; "
            cmd+= "rm -f /lib/firmware/i915/bxt_dmc_ver1.bin; "
            cmd+= "ln -s /lib/firmware/i915/bxt_dmc_ver1_07.bin /lib/firmware/i915/bxt_dmc_ver1.bin"
        else:
            cmd = "cp linux-firmware/i915/skl_dmc_ver1_27.bin /lib/firmware/i915/.; "
            cmd+= "rm -f /lib/firmware/i915/skl_dmc_ver1.bin; "
            cmd+= "ln -s /lib/firmware/i915/skl_dmc_ver1_27.bin /lib/firmware/i915/skl_dmc_ver1.bin"

        os.system(cmd)

    # Create installation path
    os.system("mkdir -p %s/build"%(WORKING_DIR))
    cmd = "mkdir -p /opt; "
    cmd+= "mkdir -p /opt/intel; "
    print cmd
    os.system(cmd)
    if enable_msdk == True:
        print ""
        print "************************************************************************"
        print_info("Pull all the source code.", loglevelcode.INFO)
        print "************************************************************************"

        # Pull all the source code
        print "libva"
        if not os.path.exists("%s/libva"%(WORKING_DIR)):
            cmd = "cd %s; git clone https://github.com/01org/libva.git"%(WORKING_DIR) 
            print cmd
            os.system(cmd);

        print "libva-utils"
        if not os.path.exists("%s/libva-utils"%(WORKING_DIR)):
            cmd = "cd %s; git clone https://github.com/01org/libva-utils.git"%(WORKING_DIR)
            print cmd
            os.system(cmd);

        print "media-driver"
        if not os.path.exists("%s/media-driver"%(WORKING_DIR)): 
            cmd = "cd %s; git clone https://github.com/intel/media-driver.git; "%(WORKING_DIR)
            print cmd
            os.system(cmd);

        print "gmmlib"
        if not os.path.exists("%s/gmmlib"%(WORKING_DIR)): 
            cmd = "cd %s; git clone https://github.com/intel/gmmlib.git; "%(WORKING_DIR)
            print cmd
            os.system(cmd);

        print "MediaSDK"
        if not os.path.exists("%s/MediaSDK"%(WORKING_DIR)): 
            cmd = "cd %s; git clone https://github.com/Intel-Media-SDK/MediaSDK.git; "%(WORKING_DIR)
            print cmd
            os.system(cmd);

        print "tbb"
        if not os.path.exists("%s/tbb"%(WORKING_DIR)): 
            cmd = "cd %s; git clone https://github.com/01org/tbb.git; "%(WORKING_DIR)
            print cmd
            os.system(cmd);

        print ""
        print "************************************************************************"
        print_info("Build and Install libVA", loglevelcode.INFO)
        print "************************************************************************"

        # Build and install libVA including the libVA utils for vainfo.
        cmd ="cd %s/libva; "%(WORKING_DIR)
        cmd+="git checkout 285267586a3d4db0e721d30d4a5f5f9fe6a3c913; "
        cmd+="./autogen.sh --prefix=/usr --libdir=/usr/lib/x86_64-linux-gnu; make -j4; make install"
        print cmd
        os.system(cmd)

        cmd ="cd %s/libva-utils; "%(WORKING_DIR)
        cmd+="git checkout 375e4eaae3377c1806e83874f9fa9b79b1f225b1; "
        cmd+="./autogen.sh --prefix=/usr --libdir=/usr/lib/x86_64-linux-gnu; make -j4; make install"
        print cmd
        os.system(cmd)

        print ""		
        print "************************************************************************"
        print_info("Build and Install media driver", loglevelcode.INFO)
        print "************************************************************************"

        # Build and install media driver    
        cmd = "cd %s/gmmlib; "%(WORKING_DIR)
        cmd+= "git checkout 5cd8dca50b1f6ddbce229098dc2bec55fc922fa7"
        print cmd
        os.system(cmd)

        cmd = "cd %s/media-driver; "%(WORKING_DIR)
        cmd+= "git checkout 840c756952419510566248734138c68b5b6bc76f"
        print cmd
        os.system(cmd)
        cmd = "mkdir %s/build; "%(WORKING_DIR)
        cmd = "cd %s/build; "%(WORKING_DIR)
        cmd+= "cmake ../media-driver; "
        cmd+= "make -j4; make install"
        print cmd
        os.system(cmd)

        print ""
        print "************************************************************************"
        print_info("Build and Install Media SDK and samples", loglevelcode.INFO)
        print "************************************************************************"

        print ""
        print "************************************************************************"
        print_info("Build and Install tbb", loglevelcode.INFO)
        print "************************************************************************"

        # Build and install tbb.
        cmd = "cd %s/tbb; "%(WORKING_DIR)
        cmd+= "git checkout 4c73c3b5d7f78c40f69e0c04fd4afb9f48add1e6; "
        cmd+= "make tbb tbbmalloc"
        print cmd
        os.system(cmd)

        cmd = "cd %s/tbb; "%(WORKING_DIR)	
        cmd+= "export TBBROOT=/opt/intel/tbb; "
        cmd+= "mkdir $TBBROOT; mkdir $TBBROOT/bin; mkdir $TBBROOT/lib; "
        cmd+= "cp -r include $TBBROOT; "
        cmd+= "cp build/linux_*_release/libtbbmalloc* $TBBROOT/lib; "
        cmd+= "cp build/linux_*_release/libtbb.so* $TBBROOT/lib"
        print cmd
        os.system(cmd)

    print ""
    print "************************************************************************"
    print_info("Install Intel OpenCL driver and CV SDK", loglevelcode.INFO)
    print "************************************************************************"

    #add users to video group.  starting with sudo group members.  TODO: anyone else?
    
    if not os.path.exists("l_openvino_toolkit_p_2018.4.420"):
        print_info("downloading Intel(R) Computer Vision SDK", loglevelcode.INFO) 
        cmd="curl -# -O http://registrationcenter-download.intel.com/akdlm/irc_nas/14920/l_openvino_toolkit_p_2018.4.420.tgz"
        print cmd
        os.system(cmd)

    print_info("installing Intel(R) Computer Vision SDK RC4", loglevelcode.INFO) 
    cmd ="tar -xzf l_openvino_toolkit_p_2018.4.420.tgz;"
    cmd+="cp silent.cfg l_openvino_toolkit_p_2018.4.420;"
    cmd+="cd l_openvino_toolkit_p_2018.4.420;"
    print "i'm here1"
    cmd+="./install.sh -s silent.cfg; "
    print "i'm here2"
    print cmd
    os.system(cmd)


    print_info("installing Intel(R) OpenCL Neo driver", loglevelcode.INFO)
    cmd = "mkdir -p %s/neo; "%(WORKING_DIR)

    cmd+= "cd %s/neo; "%(WORKING_DIR)
    cmd+= "wget https://github.com/intel/compute-runtime/releases/download/18.45.11804/intel-gmmlib_18.4.0.348_amd64.deb; "
    cmd+= "wget https://github.com/intel/compute-runtime/releases/download/18.45.11804/intel-igc-core_18.44.1060_amd64.deb; "
    cmd+= "wget https://github.com/intel/compute-runtime/releases/download/18.45.11804/intel-igc-opencl_18.44.1060_amd64.deb; "
    cmd+= "wget https://github.com/intel/compute-runtime/releases/download/18.45.11804/intel-opencl_18.45.11804_amd64.deb;"
    cmd+= "dpkg -i *.deb;"  
    cmd+= "ldconfig;"
    print cmd
    os.system(cmd)

    if enable_msdk == True:
        # Build and install Media SDK library and samples
        if os.path.exists("/opt/intel/mediasdk"):
            cmd = "rm /opt/intel/mediasdk -rf" 
            print cmd
            os.system(cmd);

        cmd ="cd %s/MediaSDK; "%(WORKING_DIR)
	cmd+= "git checkout 2130888ffd1b7f3c5c4c975020840b30556786a7; "
        cmd+="export MFX_HOME=%s/MediaSDK; "%(WORKING_DIR)
        cmd+="mkdir %s/MediaSDK/build; "%(WORKING_DIR)
        cmd+="cd %s/MediaSDK/build; "%(WORKING_DIR)
        cmd+="cmake ..;make -j8;make install; "
        print cmd
        os.system(cmd)

        print ""
        print "************************************************************************"
        print_info("Set environment variables. ", loglevelcode.INFO)
        print "************************************************************************"
        cmd = "echo 'export MFX_HOME=/opt/intel/mediasdk' >> ~/.bashrc; "
        cmd+= "echo 'export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/lib/x86_64-linux-gnu:/usr/lib' >> ~/.bashrc; "
        cmd+= "echo 'export LIBVA_DRIVERS_PATH=/usr/lib/x86_64-linux-gnu/dri' >> ~/.bashrc; "
        cmd+= "echo 'export LIBVA_DRIVER_NAME=iHD' >> ~/.bashrc"
        print cmd
        os.system(cmd)

        cmd = "echo '/usr/lib/x86_64-linux-gnu' > /etc/ld.so.conf.d/libdrm-intel.conf; "
        cmd+= "echo '/usr/lib' >> /etc/ld.so.conf.d/libdrm-intel.conf; "
        cmd+= "ldconfig"
        print cmd
        os.system(cmd)

        cmd = "export TBBROOT=/opt/intel/tbb; "
        cmd+= "echo 'export TBBROOT='$TBBROOT > $TBBROOT/bin/tbbvars.sh; "
        cmd+= "echo 'export LIBRARY_PATH='$TBBROOT/lib >> $TBBROOT/bin/tbbvars.sh; "
        cmd+= "echo 'export CPATH='$TBBROOT/include >> $TBBROOT/bin/tbbvars.sh; "
        cmd+= "chmod +x $TBBROOT/bin/tbbvars.sh; "
        cmd+= "echo 'export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:'$TBBROOT/lib >> ~/.bashrc;"
        print cmd
        os.system(cmd)

    print "************************************************************************"
    print "   Environment Setup is done, start to compile video analytics example !!! "
    print "************************************************************************"

