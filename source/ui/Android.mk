LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := ui
LOCAL_MODULE_FILENAME := lib$(LOCAL_MODULE)_android_$(TARGET_ARCH_ABI)

LOCAL_C_INCLUDES := $(QFUSION_PATH)/third-party/angelscript/sdk/angelscript/include $(QFUSION_PATH)/third-party/nanosvg/src $(LOCAL_PATH)
LOCAL_PCH := ui_precompiled.h

LOCAL_CFLAGS := $(LOCAL_EXPORT_CFLAGS) -DRMLUI_STATIC_LIB=1 -DRMLUI_NO_FONT_INTERFACE_DEFAULT=1 -DRMLUI_NO_THIRDPARTY_CONTAINERS=1

LOCAL_STATIC_LIBRARIES := RmlUiCore RmlUiControls RmlUiDebugger

LOCAL_SRC_FILES := \
  ../gameshared/q_math.c \
  ../gameshared/q_shared.c \
  ../qalgo/hash.c \
  ../qalgo/md5.c \
  $(addprefix as/,$(notdir $(wildcard $(LOCAL_PATH)/as/*.cpp))) \
  $(addprefix datasources/,$(notdir $(wildcard $(LOCAL_PATH)/datasources/*.cpp))) \
  $(addprefix decorators/,$(notdir $(wildcard $(LOCAL_PATH)/decorators/*.cpp))) \
  $(addprefix kernel/,$(notdir $(wildcard $(LOCAL_PATH)/kernel/*.cpp))) \
  $(addprefix parsers/,$(notdir $(wildcard $(LOCAL_PATH)/parsers/*.cpp))) \
  $(addprefix widgets/,$(notdir $(wildcard $(LOCAL_PATH)/widgets/*.cpp))) \
  $(notdir $(wildcard $(LOCAL_PATH)/*.cpp))

include $(BUILD_SHARED_LIBRARY)
