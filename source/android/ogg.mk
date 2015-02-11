LOCAL_PATH := $(QFUSION_PATH)/libsrcs/libogg
include $(CLEAR_VARS)
LOCAL_MODULE := ogg

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_C_INCLUDES := $(LOCAL_EXPORT_C_INCLUDES)

LOCAL_SRC_FILES := \
  src/bitwise.c \
  src/framing.c

include $(BUILD_STATIC_LIBRARY)
