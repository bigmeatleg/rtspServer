###############################################################################
## Copyright (C), 2016-2017, Allwinner Tech. Co., Ltd.
## file     : Makefile  $(TOP_DIR)/src/mpp/mpp_vi_vo_venc
## brief    : for all project main makefile control
## author   : wangguixing
## versoion : V0.1  create
## date     : 2017.04.29
###############################################################################
export SCRIPTS_DIR      = ./scripts
export MPP_DIR          = /usr/include/mpp/middleware
export SDK_LIB_SO_DIR   = /usr/lib
export CROSS_COMPILE := arm-linux-gnueabi-
export DEBUG_FLAG       := Y
export STRIP_FLAG       := Y

##################################################
#
# Setup build C/CPP FLAG
#
##################################################
export CC       = $(CROSS_COMPILE)gcc
export CXX      = $(CROSS_COMPILE)g++
export AR       = $(CROSS_COMPILE)ar
export STRIP    = $(CROSS_COMPILE)strip

INC_FLAGS :=
INC_FLAGS += -I$(MPP_DIR)/../system/include
INC_FLAGS += -I$(MPP_DIR)/../system/include/vo
INC_FLAGS += -I$(MPP_DIR)/../system/include/kernel-headers
INC_FLAGS += -I$(MPP_DIR)/../system/include/libion
INC_FLAGS += -I$(MPP_DIR)/../system/include/libion/ion

INC_FLAGS += -I$(MPP_DIR)/config
INC_FLAGS += -I$(MPP_DIR)/include/utils
INC_FLAGS += -I$(MPP_DIR)/include/media
INC_FLAGS += -I$(MPP_DIR)/include/media/utils
INC_FLAGS += -I$(MPP_DIR)/include
INC_FLAGS += -I$(MPP_DIR)/media/include/utils
INC_FLAGS += -I$(MPP_DIR)/media/include/component
INC_FLAGS += -I$(MPP_DIR)/media/include/include_render
INC_FLAGS += -I$(MPP_DIR)/media/include/videoIn
INC_FLAGS += -I$(MPP_DIR)/media/include/audio
INC_FLAGS += -I$(MPP_DIR)/media/include
INC_FLAGS += -I$(MPP_DIR)/media/LIBRARY/libcedarc/include
INC_FLAGS += -I$(MPP_DIR)/media/LIBRARY/audioEncLib/include
INC_FLAGS += -I$(MPP_DIR)/media/LIBRARY/audioDecLib/include
INC_FLAGS += -I$(MPP_DIR)/media/LIBRARY/audioEffectLib/include
INC_FLAGS += -I$(MPP_DIR)/media/LIBRARY/include_demux
INC_FLAGS += -I$(MPP_DIR)/media/LIBRARY/include_muxer
INC_FLAGS += -I$(MPP_DIR)/media/LIBRARY/include_FsWriter
INC_FLAGS += -I$(MPP_DIR)/media/LIBRARY/include_stream
INC_FLAGS += -I$(MPP_DIR)/media/LIBRARY/libmuxer/mp4_muxer
INC_FLAGS += -I$(MPP_DIR)/media/LIBRARY/libcedarx/libcore/include
INC_FLAGS += -I$(MPP_DIR)/media/LIBRARY/libcedarx/libcore/base/include
INC_FLAGS += -I$(MPP_DIR)/media/LIBRARY/libcedarx/libcore/common/iniparser
INC_FLAGS += -I$(MPP_DIR)/media/LIBRARY/libcedarx/libcore/parser/include
INC_FLAGS += -I$(MPP_DIR)/media/LIBRARY/libcedarx/libcore/stream/include
INC_FLAGS += -I$(MPP_DIR)/media/LIBRARY/libcedarx
INC_FLAGS += -I$(MPP_DIR)/media/LIBRARY/include_ai_common
INC_FLAGS += -I$(MPP_DIR)/media/LIBRARY/libISE
INC_FLAGS += -I$(MPP_DIR)/media/LIBRARY/libVLPR/include
INC_FLAGS += -I$(MPP_DIR)/media/LIBRARY/libisp/include/V4l2Camera
INC_FLAGS += -I$(MPP_DIR)/media/LIBRARY/libisp/include/device
INC_FLAGS += -I$(MPP_DIR)/media/LIBRARY/libisp/include
INC_FLAGS += -I$(MPP_DIR)/media/LIBRARY/libisp/isp_dev
INC_FLAGS += -I$(MPP_DIR)/media/LIBRARY/libisp/isp_tuning
INC_FLAGS += -I$(MPP_DIR)/media/LIBRARY/libisp
INC_FLAGS += -I$(MPP_DIR)/media/LIBRARY/include_eve_common
INC_FLAGS += -I$(MPP_DIR)/media/LIBRARY/libevekernel/include
INC_FLAGS += -I$(MPP_DIR)/media/LIBRARY/libeveface/include
INC_FLAGS += -I/usr/include/json-c

LD_FLAG   :=
LD_FLAG   += -L=/lib -L=/usr/lib
LD_FLAG   += -L$(TOP_DIR)/lib -L$(SDK_LIB_SO_DIR) #-L$(SDK_LIB_A_DIR)
LD_FLAG   += -lc
LD_FLAG   += -lpthread
LD_FLAG   += -lTinyServer
LD_FLAG   += -llog
LD_FLAG	  += -ldl
#LD_FLAG   += -Wl,--allow-shlib-undefined

LD_FLAG   += -lglog
LD_FLAG   += -lasound
LD_FLAG   += -lz

ifeq ($(COMPILE_MODE), COMPILE_LIB_STATIC)
###### Static lib #######

else
###### Share lib #######
LD_FLAG   += -lcedarxrender -lmpp_vi -lmpp_isp -lmpp_ise -lmpp_vo -lmpp_component
LD_FLAG   += -lmedia_mpp -lcdx_common -lISP -lMemAdapter -lvencoder -lmedia_utils -lhwdisplay
LD_FLAG   += -l_ise_bi -l_ise_bi_soft -l_ise_mo -l_ise_sti -lion -lcedarxstream
LD_FLAG   += -lcdx_parser -lcdx_stream  -lvdecoder -lnormal_audio
LD_FLAG   += -lcutils -lcedarx_aencoder -lisp_ini  -lVE  -lcdc_base -lcdx_base
LD_FLAG   += -lvideoengine -ladecoder
LD_FLAG   += -ljson-c
endif

#LD_FLAG   += -lmpp_menu -lmenu  -lmpp_com -lcommon


#LD_FLAG   += $(TOP_DIR)/lib/libglog.so.0
#LD_FLAG   += $(TOP_DIR)/lib/libasound.so.2
#LD_FLAG   += $(TOP_DIR)/lib/libz.so.1

DEF_FLAGS += -DV5 -DV5_DVB

ifeq ($(DEBUG_FLAG), Y)
export DEBUG_FLAG = Y
CFLAGS    += -g
CXXFlAGS  += -g
DEF_FLAGS += -DINFO_DEBUG -DERR_DEBUG
#export STRIP_FLAG = N
else
export DEBUG_FLAG = N
#export STRIP_FLAG = Y
endif

export CFLAGS    += $(INC_FLAGS) $(DEF_FLAGS) $(LD_FLAG)
export CXXFlAGS  += $(INC_FLAGS) $(DEF_FLAGS) $(LD_FLAG)

export AR_FLAGS   = -rc

export BUILD_CPP_BIN     := $(SCRIPTS_DIR)/build_cpp_bin.mk
###############################################################
# You can change $(TOP_DIR)/Makefile.param of APP_BUILD_MODE
# val. SAMPLE_MODE or MENU_MODE
###############################################################
TARGET_BIN = rtsp

#COPY_TO_DIR = $(TOP_DIR)/release

#include $(BUILD_BIN)
include $(BUILD_CPP_BIN)

##################################################
#
# Make function 
#
##################################################
make_subdir =                                               \
        @if [ -e $(1) -a -d $(1) ]; then                    \
        $(CD) $(1); $(MAKE) all; $(CD) -;                   \
        else                                                \
        $(ECHO) "***Error: Folder $(1) not exist";          \
        exit -1;                                            \
        fi

make_clean_subdir =                                         \
        @if [ -e $(1) -a -d $(1) ]; then                    \
        $(CD) $(1); $(MAKE) clean; $(CD) -;                 \
        else                                                \
        $(ECHO) "Warning: Folder $(1) not exist";           \
        fi


make_all_subdir =                                           \
        @for dir in $(1);                                   \
        do                                                  \
        if [ -d $$dir ]; then                               \
        $(CD) $$dir; $(MAKE) all; $(CD) -;                  \
        fi;                                                 \
        done


make_clean_all_subdir =                                     \
        @for dir in $(1);                                   \
        do                                                  \
        if [ -d $$dir ]; then                               \
        $(CD) $$dir; $(MAKE) clean; $(CD) -;                \
        fi;                                                 \
        done