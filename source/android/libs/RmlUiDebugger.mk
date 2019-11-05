LOCAL_PATH := $(QFUSION_PATH)/third-party/libRocket/
include $(CLEAR_VARS)
LOCAL_MODULE := RmlUiDebugger

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/Include
LOCAL_C_INCLUDES := $(LOCAL_EXPORT_C_INCLUDES)

LOCAL_CFLAGS := $(LOCAL_EXPORT_CFLAGS)

LOCAL_SRC_FILES := \
  Source/Debugger/Geometry.cpp \
  Source/Debugger/Plugin.cpp \
  Source/Debugger/Debugger.cpp \
  Source/Debugger/ElementLog.cpp \
  Source/Debugger/ElementInfo.cpp \
  Source/Debugger/ElementContextHook.cpp \
  Source/Debugger/SystemInterface.cpp

include $(BUILD_STATIC_LIBRARY)
