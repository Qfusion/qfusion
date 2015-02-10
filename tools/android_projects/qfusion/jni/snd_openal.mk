LOCAL_PATH := $(call my-dir)/source
include $(CLEAR_VARS)
LOCAL_MODULE := snd_openal
LOCAL_MODULE_FILENAME := lib$(LOCAL_MODULE)_android_$(TARGET_ARCH_ABI)

LOCAL_C_INCLUDES := $(LOCAL_PATH)/snd_common $(LOCAL_PATH)/snd_openal

LOCAL_STATIC_LIBRARIES := OpenAL-MOB vorbis

LOCAL_SRC_FILES := \
  gameshared/q_math.c \
  gameshared/q_shared.c\
  snd_common/snd_cmdque.c \
  snd_openal/qal.c \
  snd_openal/snd_al.c \
  snd_openal/snd_buffers.c \
  snd_openal/snd_decoder.c \
  snd_openal/snd_decoder_ogg.c \
  snd_openal/snd_decoder_wav.c \
  snd_openal/snd_main.c \
  snd_openal/snd_music.c \
  snd_openal/snd_sources.c \
  snd_openal/snd_stream.c \
  snd_openal/snd_syscalls.c

include $(BUILD_SHARED_LIBRARY)
