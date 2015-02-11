LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := cin
LOCAL_MODULE_FILENAME := lib$(LOCAL_MODULE)_android_$(TARGET_ARCH_ABI)

LOCAL_STATIC_LIBRARIES := theora vorbis

LOCAL_SRC_FILES := \
  ../gameshared/q_math.c \
  ../gameshared/q_shared.c \
  cin.c \
  cin_main.c \
  cin_roq.c \
  cin_syscalls.c \
  cin_theora.c

include $(BUILD_SHARED_LIBRARY)
