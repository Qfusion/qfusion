LOCAL_PATH := $(QFUSION_PATH)/libsrcs/OpenAL-MOB
include $(CLEAR_VARS)
LOCAL_MODULE := OpenAL-MOB

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include $(LOCAL_PATH)/OpenAL32/Include
LOCAL_C_INCLUDES := $(LOCAL_EXPORT_C_INCLUDES) $(LOCAL_PATH)/mob/Include $(LOCAL_PATH)/build_android

LOCAL_EXPORT_LDLIBS := -lOpenSLES

LOCAL_SRC_FILES := \
  Alc/ALc.c \
  Alc/alcConfig.c \
  Alc/alcDedicated.c \
  Alc/alcEcho.c \
  Alc/alcModulator.c \
  Alc/alcReverb.c \
  Alc/alcRing.c \
  Alc/alcThread.c \
  Alc/ALu.c \
  Alc/backends/loopback.c \
  Alc/backends/null.c \
  Alc/backends/opensl.c \
  Alc/backends/wave.c \
  Alc/bs2b.c \
  Alc/helpers.c \
  Alc/hrtf.c \
  Alc/mixer.c \
  Alc/mixer_c.c \
  Alc/mixer_neon.c \
  Alc/mixer_sse.c \
  Alc/panning.c \
  mob/alConfigMob.c \
  OpenAL32/alAuxEffectSlot.c \
  OpenAL32/alBuffer.c \
  OpenAL32/alEffect.c \
  OpenAL32/alError.c \
  OpenAL32/alExtension.c \
  OpenAL32/alFilter.c \
  OpenAL32/alListener.c \
  OpenAL32/alSource.c \
  OpenAL32/alState.c \
  OpenAL32/alThunk.c

include $(BUILD_STATIC_LIBRARY)
