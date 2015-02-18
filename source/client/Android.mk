LOCAL_PATH := $(call my-dir)/..
include $(CLEAR_VARS)
LOCAL_MODULE := qfusion

LOCAL_LDLIBS := -landroid -llog -lz
LOCAL_STATIC_LIBRARIES := curl

LOCAL_SRC_FILES := \
  android/android_clipboard.c \
  android/android_console.c \
  android/android_input.c \
  android/android_lib.c \
  android/android_native_app_glue.c \
  android/android_sys.c \
  android/android_vfs.c \
  android/android_vid.c \
  $(wildcard $(LOCAL_PATH)/client/*.c) \
  gameshared/q_math.c \
  gameshared/q_shared.c \
  $(wildcard $(LOCAL_PATH)/matchmaker/*.c) \
  qalgo/base64.c \
  qalgo/glob.c \
  qalgo/hash.c \
  qalgo/md5.c \
  qalgo/q_trie.c \
  $(wildcard $(LOCAL_PATH)/qcommon/*.c) \
  $(wildcard $(LOCAL_PATH)/server/*.c) \
  unix/unix_fs.c \
  unix/unix_net.c \
  unix/unix_threads.c \
  unix/unix_time.c

include $(BUILD_SHARED_LIBRARY)
