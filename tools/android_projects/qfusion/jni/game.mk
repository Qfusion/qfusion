LOCAL_PATH := $(call my-dir)/source
include $(CLEAR_VARS)
LOCAL_MODULE := game
LOCAL_MODULE_FILENAME := lib$(LOCAL_MODULE)_android_$(TARGET_ARCH_ABI)

LOCAL_CFLAGS := -DGAME_MODULE
LOCAL_C_INCLUDES := $(QFUSION_PATH)/libsrcs/angelscript/sdk/angelscript/include

LOCAL_SRC_FILES := \
  game/g_as_gametypes.cpp \
  game/g_as_maps.cpp \
  game/g_ascript.cpp \
  game/g_awards.cpp \
  game/g_callvotes.cpp \
  game/g_chase.cpp \
  game/g_clip.cpp \
  game/g_cmds.cpp \
  game/g_combat.cpp \
  game/g_frame.cpp \
  game/g_func.cpp \
  game/g_gameteams.cpp \
  game/g_gametypes.cpp \
  game/g_items.cpp \
  game/g_main.cpp \
  game/g_misc.cpp \
  game/g_mm.cpp \
  game/g_phys.cpp \
  game/g_spawn.cpp \
  game/g_spawnpoints.cpp \
  game/g_svcmds.cpp \
  game/g_syscalls.cpp \
  game/g_target.cpp \
  game/g_trigger.cpp \
  game/g_utils.cpp \
  game/g_weapon.cpp \
  game/g_web.cpp \
  game/p_client.cpp \
  game/p_hud.cpp \
  game/p_view.cpp \
  game/p_weapon.cpp \
  game/ai/AStar.cpp \
  game/ai/bot_spawn.cpp \
  game/ai/ai_class_dmbot.cpp \
  game/ai/ai_common.cpp \
  game/ai/ai_dropnodes.cpp \
  game/ai/ai_items.cpp \
  game/ai/ai_links.cpp \
  game/ai/ai_main.cpp \
  game/ai/ai_movement.cpp \
  game/ai/ai_navigation.cpp \
  game/ai/ai_nodes.cpp \
  game/ai/ai_tools.cpp \
  gameshared/gs_gameteams.c \
  gameshared/gs_items.c \
  gameshared/gs_misc.c \
  gameshared/gs_players.c \
  gameshared/gs_pmove.c \
  gameshared/gs_slidebox.c \
  gameshared/gs_weapondefs.c \
  gameshared/gs_weapons.c \
  gameshared/q_math.c \
  gameshared/q_shared.c \
  matchmaker/mm_rating.c

include $(BUILD_SHARED_LIBRARY)
