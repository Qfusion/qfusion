LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := cgame
LOCAL_MODULE_FILENAME := lib$(LOCAL_MODULE)_android_$(TARGET_ARCH_ABI)

LOCAL_CFLAGS := -DCGAME_MODULE
LOCAL_C_INCLUDES := $(QFUSION_PATH)/third-party/angelscript/sdk/angelscript/include

LOCAL_SRC_FILES := \
  ../gameshared/gs_ascript.cpp \
  ../gameshared/gs_gameteams.c \
  ../gameshared/gs_items.c \
  ../gameshared/gs_misc.c \
  ../gameshared/gs_players.c \
  ../gameshared/gs_pmove.cpp \
  ../gameshared/gs_slidebox.c \
  ../gameshared/gs_weapondefs.c \
  ../gameshared/gs_weapons.c \
  ../gameshared/q_math.c \
  ../gameshared/q_shared.c \
  $(notdir $(wildcard $(LOCAL_PATH)/*.cpp))

include $(BUILD_SHARED_LIBRARY)
