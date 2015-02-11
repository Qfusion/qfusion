LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := game
LOCAL_MODULE_FILENAME := lib$(LOCAL_MODULE)_android_$(TARGET_ARCH_ABI)

LOCAL_CFLAGS := -DGAME_MODULE
LOCAL_C_INCLUDES := $(QFUSION_PATH)/libsrcs/angelscript/sdk/angelscript/include

LOCAL_SRC_FILES := \
  ../gameshared/gs_gameteams.c \
  ../gameshared/gs_items.c \
  ../gameshared/gs_misc.c \
  ../gameshared/gs_players.c \
  ../gameshared/gs_pmove.c \
  ../gameshared/gs_slidebox.c \
  ../gameshared/gs_weapondefs.c \
  ../gameshared/gs_weapons.c \
  ../gameshared/q_math.c \
  ../gameshared/q_shared.c \
  ../matchmaker/mm_rating.c \
  ai/AStar.cpp \
  ai/bot_spawn.cpp \
  ai/ai_class_dmbot.cpp \
  ai/ai_common.cpp \
  ai/ai_dropnodes.cpp \
  ai/ai_items.cpp \
  ai/ai_links.cpp \
  ai/ai_main.cpp \
  ai/ai_movement.cpp \
  ai/ai_navigation.cpp \
  ai/ai_nodes.cpp \
  ai/ai_tools.cpp \
  g_as_gametypes.cpp \
  g_as_maps.cpp \
  g_ascript.cpp \
  g_awards.cpp \
  g_callvotes.cpp \
  g_chase.cpp \
  g_clip.cpp \
  g_cmds.cpp \
  g_combat.cpp \
  g_frame.cpp \
  g_func.cpp \
  g_gameteams.cpp \
  g_gametypes.cpp \
  g_items.cpp \
  g_main.cpp \
  g_misc.cpp \
  g_mm.cpp \
  g_phys.cpp \
  g_spawn.cpp \
  g_spawnpoints.cpp \
  g_svcmds.cpp \
  g_syscalls.cpp \
  g_target.cpp \
  g_trigger.cpp \
  g_utils.cpp \
  g_weapon.cpp \
  g_web.cpp \
  p_client.cpp \
  p_hud.cpp \
  p_view.cpp \
  p_weapon.cpp

include $(BUILD_SHARED_LIBRARY)
