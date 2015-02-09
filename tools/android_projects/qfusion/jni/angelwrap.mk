LOCAL_PATH := $(call my-dir)/source
include $(CLEAR_VARS)
LOCAL_MODULE := angelwrap
LOCAL_MODULE_FILENAME := lib$(LOCAL_MODULE)_android_$(TARGET_ARCH_ABI)

LOCAL_C_INCLUDES := $(QFUSION_PATH)/libsrcs/angelscript/sdk/add_on/scriptarray

LOCAL_STATIC_LIBRARIES := angelscript

LOCAL_SRC_FILES := \
  gameshared/q_math.c \
  gameshared/q_shared.c\
  angelwrap/qas_angelwrap.cpp \
  angelwrap/qas_main.cpp \
  angelwrap/qas_precompiled.cpp \
  angelwrap/qas_syscalls.cpp \
  angelwrap/addon/addon_any.cpp \
  angelwrap/addon/addon_cvar.cpp \
  angelwrap/addon/addon_dictionary.cpp \
  angelwrap/addon/addon_math.cpp \
  angelwrap/addon/addon_scriptarray.cpp \
  angelwrap/addon/addon_string.cpp \
  angelwrap/addon/addon_stringutils.cpp \
  angelwrap/addon/addon_time.cpp \
  angelwrap/addon/addon_vec3.cpp

include $(BUILD_SHARED_LIBRARY)
