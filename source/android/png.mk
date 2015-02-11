LOCAL_PATH := $(QFUSION_PATH)/libsrcs/libpng
include $(CLEAR_VARS)
LOCAL_MODULE := png

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)

LOCAL_EXPORT_LDLIBS := -lz

LOCAL_SRC_FILES := \
  png.c \
  pngerror.c \
  pngget.c \
  pngmem.c \
  pngpread.c \
  pngread.c \
  pngrio.c \
  pngrtran.c \
  pngrutil.c \
  pngset.c \
  pngtrans.c \
  pngwio.c \
  pngwrite.c \
  pngwtran.c \
  pngwutil.c

include $(BUILD_STATIC_LIBRARY)
