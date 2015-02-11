LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := angelwrap
LOCAL_MODULE_FILENAME := lib$(LOCAL_MODULE)_android_$(TARGET_ARCH_ABI)

LOCAL_C_INCLUDES := $(QFUSION_PATH)/libsrcs/angelscript/sdk/add_on/scriptarray

LOCAL_STATIC_LIBRARIES := angelscript

LOCAL_SRC_FILES := \
  ../gameshared/q_math.c \
  ../gameshared/q_shared.c \
  addon/addon_any.cpp \
  addon/addon_cvar.cpp \
  addon/addon_dictionary.cpp \
  addon/addon_math.cpp \
  addon/addon_scriptarray.cpp \
  addon/addon_string.cpp \
  addon/addon_stringutils.cpp \
  addon/addon_time.cpp \
  addon/addon_vec3.cpp \
  qas_angelwrap.cpp \
  qas_main.cpp \
  qas_precompiled.cpp \
  qas_syscalls.cpp

include $(BUILD_SHARED_LIBRARY)
