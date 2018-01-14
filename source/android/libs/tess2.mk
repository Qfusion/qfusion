LOCAL_PATH := $(QFUSION_PATH)/third-party/libtess2
include $(CLEAR_VARS)
LOCAL_MODULE := tess2

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/Include
LOCAL_C_INCLUDES := $(LOCAL_EXPORT_C_INCLUDES)

LOCAL_SRC_FILES := $(addprefix Source/,$(notdir $(wildcard $(LOCAL_PATH)/Source/*.c)))

include $(BUILD_STATIC_LIBRARY)
