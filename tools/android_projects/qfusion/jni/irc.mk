LOCAL_PATH := $(call my-dir)/source
include $(CLEAR_VARS)
LOCAL_MODULE := irc
LOCAL_MODULE_FILENAME := lib$(LOCAL_MODULE)_android_$(TARGET_ARCH_ABI)

LOCAL_SRC_FILES := \
  gameshared/q_shared.c \
  irc/irc_client.c \
  irc/irc_common.c \
  irc/irc_gui.c \
  irc/irc_interface.c \
  irc/irc_listeners.c \
  irc/irc_logic.c \
  irc/irc_net.c \
  irc/irc_protocol.c \
  irc/irc_rcon.c

include $(BUILD_SHARED_LIBRARY)
