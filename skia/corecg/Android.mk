LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES:= \
	Sk64.cpp \
	SkBuffer.cpp \
	SkChunkAlloc.cpp \
	SkCordic.cpp \
	SkDebug.cpp \
	SkDebug_stdio.cpp \
	SkFloatBits.cpp \
	SkInterpolator.cpp \
	SkMath.cpp \
	SkMatrix.cpp \
	SkMemory_stdlib.cpp \
	SkPageFlipper.cpp \
	SkPoint.cpp \
	SkRect.cpp \
	SkRegion.cpp

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libutils

LOCAL_C_INCLUDES += \
	include/corecg

#LOCAL_CFLAGS+= 
#LOCAL_LDFLAGS:= 

LOCAL_MODULE:= libcorecg

LOCAL_CFLAGS += -fstrict-aliasing

ifeq ($(TARGET_ARCH),arm)
	LOCAL_CFLAGS += -fomit-frame-pointer
endif

include $(BUILD_SHARED_LIBRARY)
