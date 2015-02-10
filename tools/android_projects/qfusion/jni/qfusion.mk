LOCAL_PATH := $(call my-dir)/source
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
  client/cin.c \
  client/cl_cin.c \
  client/cl_demo.c \
  client/cl_game.c \
  client/cl_input.c \
  client/cl_main.c \
  client/cl_mm.c \
  client/cl_parse.c \
  client/cl_screen.c \
  client/cl_serverlist.c \
  client/cl_sound.c \
  client/cl_ui.c \
  client/cl_vid.c \
  client/console.c \
  client/ftlib.c \
  client/keys.c \
  client/l10n.c \
  gameshared/q_math.c \
  gameshared/q_shared.c \
  matchmaker/mm_common.c \
  matchmaker/mm_query.c \
  matchmaker/mm_rating.c \
  qalgo/base64.c \
  qalgo/glob.c \
  qalgo/hash.c \
  qalgo/md5.c \
  qalgo/q_trie.c \
  qcommon/anticheat.c \
  qcommon/ascript.c \
  qcommon/asyncstream.c \
  qcommon/bsp.c \
  qcommon/cjson.c \
  qcommon/cm_main.c \
  qcommon/cm_q3bsp.c \
  qcommon/cm_trace.c \
  qcommon/cmd.c \
  qcommon/common.c \
  qcommon/steam.c \
  qcommon/threads.c \
  qcommon/cvar.c \
  qcommon/dynvar.c \
  qcommon/files.c \
  qcommon/irc.c \
  qcommon/library.c \
  qcommon/mem.c \
  qcommon/mlist.c \
  qcommon/msg.c \
  qcommon/net.c \
  qcommon/net_chan.c \
  qcommon/patch.c \
  qcommon/snap_demos.c \
  qcommon/snap_read.c \
  qcommon/snap_write.c \
  qcommon/svnrev.c \
  qcommon/sys_vfs_zip.c \
  qcommon/webdownload.c \
  qcommon/wswcurl.c \
  server/sv_ccmds.c \
  server/sv_client.c \
  server/sv_demos.c \
  server/sv_game.c \
  server/sv_init.c \
  server/sv_main.c \
  server/sv_mm.c \
  server/sv_motd.c \
  server/sv_oob.c \
  server/sv_send.c \
  server/sv_web.c \
  unix/unix_fs.c \
  unix/unix_net.c \
  unix/unix_threads.c \
  unix/unix_time.c

include $(BUILD_SHARED_LIBRARY)
