LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := ui
LOCAL_MODULE_FILENAME := lib$(LOCAL_MODULE)_android_$(TARGET_ARCH_ABI)

LOCAL_C_INCLUDES := $(QFUSION_PATH)/libsrcs/angelscript/sdk/angelscript/include $(LOCAL_PATH)
LOCAL_PCH := ui_precompiled.h

LOCAL_STATIC_LIBRARIES := RocketCore RocketControls

LOCAL_SRC_FILES := \
  ../gameshared/q_math.c \
  ../gameshared/q_shared.c \
  ../qalgo/hash.c \
  ../qalgo/md5.c \
  as/as_bind_datasource.cpp \
  as/as_bind_demoinfo.cpp \
  as/as_bind_dom.cpp \
  as/as_bind_downloadinfo.cpp \
  as/as_bind_game.cpp \
  as/as_bind_irc.cpp \
  as/as_bind_l10n.cpp \
  as/as_bind_main.cpp \
  as/as_bind_mm.cpp \
  as/as_bind_options.cpp \
  as/as_bind_serverbrowser.cpp \
  as/as_bind_url.cpp \
  as/as_bind_window.cpp \
  as/asmodule.cpp \
  as/asui_scheduled.cpp \
  as/asui_scriptdocument.cpp \
  as/asui_scriptevent.cpp \
  datasources/ui_crosshair_datasource.cpp \
  datasources/ui_demos_datasource.cpp \
  datasources/ui_gameajax_datasource.cpp \
  datasources/ui_gametypes_datasource.cpp \
  datasources/ui_huds_datasource.cpp \
  datasources/ui_ircchannels_datasource.cpp \
  datasources/ui_maps_datasource.cpp \
  datasources/ui_models_datasource.cpp \
  datasources/ui_mods_datasource.cpp \
  datasources/ui_profiles_datasource.cpp \
  datasources/ui_serverbrowser_datasource.cpp \
  datasources/ui_tvchannels_datasource.cpp \
  datasources/ui_video_datasource.cpp \
  decorators/ui_gradient_decorator.cpp \
  decorators/ui_ninepatch_decorator.cpp \
  kernel/ui_boneposes.cpp \
  kernel/ui_common.cpp \
  kernel/ui_demoinfo.cpp \
  kernel/ui_documentloader.cpp \
  kernel/ui_eventlistener.cpp \
  kernel/ui_fileinterface.cpp \
  kernel/ui_keyconverter.cpp \
  kernel/ui_main.cpp \
  kernel/ui_polyallocator.cpp \
  kernel/ui_renderinterface.cpp \
  kernel/ui_rocketmodule.cpp \
  kernel/ui_streamcache.cpp \
  kernel/ui_systeminterface.cpp \
  kernel/ui_utils.cpp \
  parsers/ui_parsersound.cpp \
  ui_precompiled.cpp \
  ui_public.cpp \
  widgets/ui_anchor.cpp \
  widgets/ui_colorselector.cpp \
  widgets/ui_dataspinner.cpp \
  widgets/ui_field.cpp \
  widgets/ui_idiv.cpp \
  widgets/ui_iframe.cpp \
  widgets/ui_image.cpp \
  widgets/ui_irc.cpp \
  widgets/ui_keyselect.cpp \
  widgets/ui_l10n.cpp \
  widgets/ui_levelshot.cpp \
  widgets/ui_modelview.cpp \
  widgets/ui_optionsform.cpp \
  widgets/ui_selectable_datagrid.cpp \
  widgets/ui_video.cpp \
  widgets/ui_worldview.cpp

include $(BUILD_SHARED_LIBRARY)
