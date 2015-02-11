LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := snd_qf
LOCAL_MODULE_FILENAME := lib$(LOCAL_MODULE)_android_$(TARGET_ARCH_ABI)

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../snd_common $(LOCAL_PATH)

LOCAL_LDLIBS := -lOpenSLES
LOCAL_STATIC_LIBRARIES := vorbis

LOCAL_SRC_FILES := \
  ../android/android_snd.c \
  ../gameshared/q_math.c \
  ../gameshared/q_shared.c \
  ../snd_common/snd_cmdque.c \
  snd_dma.c \
  snd_main.c \
  snd_mem.c \
  snd_mix.c \
  snd_music.c \
  snd_ogg.c \
  snd_syscalls.c

include $(BUILD_SHARED_LIBRARY)
