LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := ui
LOCAL_MODULE_FILENAME := lib$(LOCAL_MODULE)_android_$(TARGET_ARCH_ABI)

LOCAL_C_INCLUDES := $(QFUSION_PATH)/libsrcs/angelscript/sdk/angelscript/include $(LOCAL_PATH)
LOCAL_PCH := ui_precompiled.h

LOCAL_STATIC_LIBRARIES := RocketCore RocketControls

LOCAL_SRC_FILES := \
  ../gameshared/q_math.c \
  ../gameshared/q_shared.c \
  ../qalgo/hash.c \
  ../qalgo/md5.c \
  $(wildcard $(LOCAL_PATH)/as/*.cpp) \
  $(wildcard $(LOCAL_PATH)/datasources/*.cpp) \
  $(wildcard $(LOCAL_PATH)/decorators/*.cpp) \
  $(wildcard $(LOCAL_PATH)/kernel/*.cpp) \
  $(wildcard $(LOCAL_PATH)/parsers/*.cpp) \
  $(wildcard $(LOCAL_PATH)/widgets/*.cpp) \
  $(wildcard $(LOCAL_PATH)/*.cpp)

include $(BUILD_SHARED_LIBRARY)
