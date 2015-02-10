LOCAL_PATH := $(call my-dir)/source
include $(CLEAR_VARS)
LOCAL_MODULE := ui
LOCAL_MODULE_FILENAME := lib$(LOCAL_MODULE)_android_$(TARGET_ARCH_ABI)

LOCAL_C_INCLUDES := $(QFUSION_PATH)/libsrcs/angelscript/sdk/angelscript/include $(LOCAL_PATH)/ui
LOCAL_PCH := ui/ui_precompiled.h

LOCAL_STATIC_LIBRARIES := RocketCore RocketControls

LOCAL_SRC_FILES := \
  gameshared/q_math.c \
  gameshared/q_shared.c \
  qalgo/hash.c \
  qalgo/md5.c \
  ui/as/as_bind_datasource.cpp \
  ui/as/as_bind_demoinfo.cpp \
  ui/as/as_bind_dom.cpp \
  ui/as/as_bind_downloadinfo.cpp \
  ui/as/as_bind_game.cpp \
  ui/as/as_bind_irc.cpp \
  ui/as/as_bind_l10n.cpp \
  ui/as/as_bind_main.cpp \
  ui/as/as_bind_mm.cpp \
  ui/as/as_bind_options.cpp \
  ui/as/as_bind_serverbrowser.cpp \
  ui/as/as_bind_url.cpp \
  ui/as/as_bind_window.cpp \
  ui/as/asmodule.cpp \
  ui/as/asui_scheduled.cpp \
  ui/as/asui_scriptdocument.cpp \
  ui/as/asui_scriptevent.cpp \
  ui/datasources/ui_crosshair_datasource.cpp \
  ui/datasources/ui_demos_datasource.cpp \
  ui/datasources/ui_gameajax_datasource.cpp \
  ui/datasources/ui_gametypes_datasource.cpp \
  ui/datasources/ui_huds_datasource.cpp \
  ui/datasources/ui_ircchannels_datasource.cpp \
  ui/datasources/ui_maps_datasource.cpp \
  ui/datasources/ui_models_datasource.cpp \
  ui/datasources/ui_mods_datasource.cpp \
  ui/datasources/ui_profiles_datasource.cpp \
  ui/datasources/ui_serverbrowser_datasource.cpp \
  ui/datasources/ui_tvchannels_datasource.cpp \
  ui/datasources/ui_video_datasource.cpp \
  ui/decorators/ui_gradient_decorator.cpp \
  ui/decorators/ui_ninepatch_decorator.cpp \
  ui/kernel/ui_boneposes.cpp \
  ui/kernel/ui_common.cpp \
  ui/kernel/ui_demoinfo.cpp \
  ui/kernel/ui_documentloader.cpp \
  ui/kernel/ui_eventlistener.cpp \
  ui/kernel/ui_fileinterface.cpp \
  ui/kernel/ui_keyconverter.cpp \
  ui/kernel/ui_main.cpp \
  ui/kernel/ui_polyallocator.cpp \
  ui/kernel/ui_renderinterface.cpp \
  ui/kernel/ui_rocketmodule.cpp \
  ui/kernel/ui_streamcache.cpp \
  ui/kernel/ui_systeminterface.cpp \
  ui/kernel/ui_utils.cpp \
  ui/parsers/ui_parsersound.cpp \
  ui/widgets/ui_anchor.cpp \
  ui/widgets/ui_colorselector.cpp \
  ui/widgets/ui_dataspinner.cpp \
  ui/widgets/ui_field.cpp \
  ui/widgets/ui_idiv.cpp \
  ui/widgets/ui_iframe.cpp \
  ui/widgets/ui_image.cpp \
  ui/widgets/ui_irc.cpp \
  ui/widgets/ui_keyselect.cpp \
  ui/widgets/ui_l10n.cpp \
  ui/widgets/ui_levelshot.cpp \
  ui/widgets/ui_modelview.cpp \
  ui/widgets/ui_optionsform.cpp \
  ui/widgets/ui_selectable_datagrid.cpp \
  ui/widgets/ui_video.cpp \
  ui/widgets/ui_worldview.cpp \
  ui/ui_precompiled.cpp \
  ui/ui_public.cpp

include $(BUILD_SHARED_LIBRARY)
