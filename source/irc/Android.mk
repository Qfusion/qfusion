LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := irc
LOCAL_MODULE_FILENAME := lib$(LOCAL_MODULE)_android_$(TARGET_ARCH_ABI)

LOCAL_SRC_FILES := \
  ../gameshared/q_shared.c \
  irc_client.c \
  irc_common.c \
  irc_gui.c \
  irc_interface.c \
  irc_listeners.c \
  irc_logic.c \
  irc_net.c \
  irc_protocol.c \
  irc_rcon.c

include $(BUILD_SHARED_LIBRARY)
