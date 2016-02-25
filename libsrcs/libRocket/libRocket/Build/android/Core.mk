LOCAL_PATH := $(call my-dir)/../..
include $(CLEAR_VARS)
LOCAL_MODULE := RocketCore

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/Include
LOCAL_C_INCLUDES := $(LOCAL_EXPORT_C_INCLUDES)
LOCAL_PCH := Source/Core/precompiled.h

LOCAL_SRC_FILES := $(addprefix Source/Core/,$(notdir $(wildcard $(LOCAL_PATH)/Source/Core/*.cpp)))

include $(BUILD_STATIC_LIBRARY)
