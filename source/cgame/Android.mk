LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := cgame
LOCAL_MODULE_FILENAME := lib$(LOCAL_MODULE)_android_$(TARGET_ARCH_ABI)

LOCAL_CFLAGS := -DCGAME_MODULE

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
  cg_boneposes.cpp \
  cg_chat.cpp \
  cg_cmds.cpp \
  cg_damage_indicator.cpp \
  cg_decals.cpp \
  cg_democams.cpp \
  cg_draw.cpp \
  cg_effects.cpp \
  cg_ents.cpp \
  cg_events.cpp \
  cg_hud.cpp \
  cg_lents.cpp \
  cg_main.cpp \
  cg_media.cpp \
  cg_players.cpp \
  cg_pmodels.cpp \
  cg_polys.cpp \
  cg_predict.cpp \
  cg_scoreboard.cpp \
  cg_screen.cpp \
  cg_syscalls.cpp \
  cg_teams.cpp \
  cg_test.cpp \
  cg_view.cpp \
  cg_vweap.cpp \
  cg_wmodels.cpp

include $(BUILD_SHARED_LIBRARY)
