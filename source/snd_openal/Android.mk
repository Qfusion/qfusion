LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := snd_openal
LOCAL_MODULE_FILENAME := lib$(LOCAL_MODULE)_android_$(TARGET_ARCH_ABI)

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../snd_common $(LOCAL_PATH)

LOCAL_STATIC_LIBRARIES := OpenAL-MOB vorbis

LOCAL_SRC_FILES := \
  ../gameshared/q_math.c \
  ../gameshared/q_shared.c \
  ../snd_common/snd_cmdque.c \
  qal.c \
  snd_al.c \
  snd_buffers.c \
  snd_decoder.c \
  snd_decoder_ogg.c \
  snd_decoder_wav.c \
  snd_main.c \
  snd_music.c \
  snd_sources.c \
  snd_stream.c \
  snd_syscalls.c

include $(BUILD_SHARED_LIBRARY)
