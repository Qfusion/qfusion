LOCAL_PATH := $(call my-dir)/source
include $(CLEAR_VARS)
LOCAL_MODULE := cgame
LOCAL_MODULE_FILENAME := lib$(LOCAL_MODULE)_android_$(TARGET_ARCH_ABI)

LOCAL_CFLAGS := -DCGAME_MODULE

LOCAL_SRC_FILES := \
  cgame/cg_boneposes.cpp \
  cgame/cg_chat.cpp \
  cgame/cg_cmds.cpp \
  cgame/cg_damage_indicator.cpp \
  cgame/cg_decals.cpp \
  cgame/cg_democams.cpp \
  cgame/cg_draw.cpp \
  cgame/cg_effects.cpp \
  cgame/cg_ents.cpp \
  cgame/cg_events.cpp \
  cgame/cg_hud.cpp \
  cgame/cg_lents.cpp \
  cgame/cg_main.cpp \
  cgame/cg_media.cpp \
  cgame/cg_players.cpp \
  cgame/cg_pmodels.cpp \
  cgame/cg_polys.cpp \
  cgame/cg_predict.cpp \
  cgame/cg_scoreboard.cpp \
  cgame/cg_screen.cpp \
  cgame/cg_syscalls.cpp \
  cgame/cg_teams.cpp \
  cgame/cg_test.cpp \
  cgame/cg_view.cpp \
  cgame/cg_vweap.cpp \
  cgame/cg_wmodels.cpp \
  gameshared/gs_gameteams.c \
  gameshared/gs_items.c \
  gameshared/gs_misc.c \
  gameshared/gs_players.c \
  gameshared/gs_pmove.c \
  gameshared/gs_slidebox.c \
  gameshared/gs_weapondefs.c \
  gameshared/gs_weapons.c \
  gameshared/q_math.c \
  gameshared/q_shared.c

include $(BUILD_SHARED_LIBRARY)
