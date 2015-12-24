LOCAL_PATH := $(QFUSION_PATH)/libsrcs/libtheora
include $(CLEAR_VARS)
LOCAL_MODULE := theora

LOCAL_CFLAGS := -DLIBTHEORA_EXPORTS -Wno-aggressive-loop-optimizations
ifeq ($(TARGET_ARCH_ABI),x86)
  LOCAL_CFLAGS += -DOC_X86_ASM
endif
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_C_INCLUDES := \
  $(LOCAL_EXPORT_C_INCLUDES) \
  $(LOCAL_PATH)/lib \
  $(QFUSION_PATH)/source/android/libs/libtheora/lib

LOCAL_STATIC_LIBRARIES := ogg

LOCAL_SRC_FILES := \
  lib/analyze.c \
  lib/apiwrapper.c \
  lib/bitpack.c \
  lib/cpu.c \
  lib/decapiwrapper.c \
  lib/decinfo.c \
  lib/decode.c \
  lib/dequant.c \
  lib/encapiwrapper.c \
  lib/encfrag.c \
  lib/encinfo.c \
  lib/encode.c \
  lib/enquant.c \
  lib/fdct.c \
  lib/fragment.c \
  lib/huffdec.c \
  lib/huffenc.c \
  lib/idct.c \
  lib/info.c \
  lib/internal.c \
  lib/mathops.c \
  lib/mcenc.c \
  lib/quant.c \
  lib/rate.c \
  lib/state.c \
  lib/tokenize.c \
  lib/x86/mmxencfrag.c \
  lib/x86/mmxfdct.c \
  lib/x86/mmxfrag.c \
  lib/x86/mmxidct.c \
  lib/x86/mmxstate.c \
  lib/x86/sse2fdct.c \
  lib/x86/x86enc.c \
  lib/x86/x86state.c

include $(BUILD_STATIC_LIBRARY)
