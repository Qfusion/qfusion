LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := angelwrap
LOCAL_MODULE_FILENAME := lib$(LOCAL_MODULE)_android_$(TARGET_ARCH_ABI)

LOCAL_C_INCLUDES := $(QFUSION_PATH)/libsrcs/angelscript/sdk/add_on/scriptarray

LOCAL_STATIC_LIBRARIES := angelscript

LOCAL_SRC_FILES := \
  ../gameshared/q_math.c \
  ../gameshared/q_shared.c \
  $(addprefix addon/,$(notdir $(wildcard $(LOCAL_PATH)/addon/*.cpp))) \
  $(notdir $(wildcard $(LOCAL_PATH)/*.c)) \
  $(notdir $(wildcard $(LOCAL_PATH)/*.cpp))

include $(BUILD_SHARED_LIBRARY)
