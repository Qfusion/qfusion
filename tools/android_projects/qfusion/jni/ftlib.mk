LOCAL_PATH := $(call my-dir)/source
include $(CLEAR_VARS)
LOCAL_MODULE := ftlib
LOCAL_MODULE_FILENAME := lib$(LOCAL_MODULE)_android_$(TARGET_ARCH_ABI)

LOCAL_STATIC_LIBRARIES := freetype

LOCAL_SRC_FILES := \
  ftlib/ftlib.c \
  ftlib/ftlib_draw.c \
  ftlib/ftlib_main.c \
  ftlib/ftlib_syscalls.c \
  gameshared/q_math.c \
  gameshared/q_shared.c

include $(BUILD_SHARED_LIBRARY)
