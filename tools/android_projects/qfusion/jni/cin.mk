LOCAL_PATH := $(call my-dir)/source
include $(CLEAR_VARS)
LOCAL_MODULE := cin
LOCAL_MODULE_FILENAME := lib$(LOCAL_MODULE)_android_$(TARGET_ARCH_ABI)

LOCAL_STATIC_LIBRARIES := theora vorbis

LOCAL_SRC_FILES := \
  cin/cin.c \
  cin/cin_main.c \
  cin/cin_roq.c \
  cin/cin_syscalls.c \
  cin/cin_theora.c \
  gameshared/q_math.c \
  gameshared/q_shared.c

include $(BUILD_SHARED_LIBRARY)
