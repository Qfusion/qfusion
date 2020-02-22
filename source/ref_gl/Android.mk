LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := ref_gl
LOCAL_MODULE_FILENAME := lib$(LOCAL_MODULE)_android_$(TARGET_ARCH_ABI)

LOCAL_LDLIBS := -landroid
LOCAL_C_INCLUDES := $(QFUSION_PATH)/third-party/stb $(LOCAL_PATH)

LOCAL_SRC_FILES := \
  ../egl/egl_glw.c \
  ../egl/egl_qgl.c \
  ../gameshared/q_math.c \
  ../gameshared/q_shared.c \
  ../qalgo/glob.c \
  ../qalgo/hash.c \
  ../qalgo/half_float.c \
  ../qalgo/q_trie.c \
  ../qcommon/bsp.c \
  ../qcommon/patch.c \
  $(notdir $(wildcard $(LOCAL_PATH)/*.c))

include $(BUILD_SHARED_LIBRARY)
