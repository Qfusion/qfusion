# Copyright (C) 2009 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../include $(LOCAL_PATH)/../../OpenAL32/Include $(LOCAL_PATH)/../../mob/Include
LOCAL_MODULE    := OpenAL-MOB
LOCAL_SRC_FILES := ../../Alc/ALc.c ../../Alc/alcConfig.c ../../Alc/alcDedicated.c ../../Alc/alcEcho.c ../../Alc/alcModulator.c ../../Alc/alcReverb.c ../../Alc/alcRing.c ../../Alc/alcThread.c ../../Alc/ALu.c ../../Alc/backends/loopback.c ../../Alc/backends/null.c ../../Alc/backends/opensl.c ../../Alc/backends/wave.c ../../Alc/bs2b.c ../../Alc/helpers.c ../../Alc/hrtf.c ../../Alc/mixer.c ../../Alc/mixer_c.c ../../Alc/mixer_neon.c ../../Alc/mixer_sse.c ../../Alc/panning.c ../../mob/alConfigMob.c ../../OpenAL32/alAuxEffectSlot.c ../../OpenAL32/alBuffer.c ../../OpenAL32/alEffect.c ../../OpenAL32/alError.c ../../OpenAL32/alExtension.c ../../OpenAL32/alFilter.c ../../OpenAL32/alListener.c ../../OpenAL32/alSource.c ../../OpenAL32/alState.c ../../OpenAL32/alThunk.c

# set the platform flags
ifeq ($(APP_ABI),x86)
	LOCAL_CFLAGS += -D HAVE_SSE
else
	LOCAL_CFLAGS += -D HAVE_NEON -mfloat-abi=softfp -mfpu=neon -marm
endif

#LOCAL_SHARED_LIBRARIES += libOpenSLES
LOCAL_LDFLAGS += -lOpenSLES

include $(BUILD_SHARED_LIBRARY)
