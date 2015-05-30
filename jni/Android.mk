LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)					# clean everything up to prepare for a module
LOCAL_ARM_MODE  := arm					# full speed arm instead of thumb
LOCAL_ARM_NEON  := true					# compile with neon support enabled
LOCAL_MODULE := jpeg
LOCAL_SRC_FILES := libjpeg.a

include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)					# clean everything up to prepare for a module
include ../../VRLib/import_vrlib.mk		# import VRLib for this module.  Do NOT call $(CLEAR_VARS) until after building your module.
										# use += instead of := when defining the following variables: LOCAL_LDLIBS, LOCAL_CFLAGS, LOCAL_C_INCLUDES, LOCAL_STATIC_LIBRARIES 

include ../../VRLib/cflags.mk

LOCAL_MODULE    := oculus360videos		# generate oculus360videos.so
LOCAL_SRC_FILES  := Oculus360Videos.cpp VideoBrowser.cpp VideoMenu.cpp VideosMetaData.cpp OVR_TurboJpeg.cpp

LOCAL_STATIC_LIBRARIES += jpeg

include $(BUILD_SHARED_LIBRARY)			# start building based on everything since CLEAR_VARS
