QFUSION_PATH := $(call my-dir)

# Set this to the name of your game version header.
# QFUSION_APPLICATION_VERSION_HEADER := version.warsow.h

include $(NDK_ROOT)/sources/android/native_app_glue/Android.mk

include $(QFUSION_PATH)/source/android/libs/angelscript.mk
include $(QFUSION_PATH)/source/android/libs/curl.mk
include $(QFUSION_PATH)/source/android/libs/freetype.mk
include $(QFUSION_PATH)/source/android/libs/OpenAL-MOB.mk
include $(QFUSION_PATH)/source/android/libs/openssl-crypto.mk
include $(QFUSION_PATH)/source/android/libs/openssl-ssl.mk

include $(QFUSION_PATH)/source/android/libs/RmlUiControls.mk
include $(QFUSION_PATH)/source/android/libs/RmlUiCore.mk
include $(QFUSION_PATH)/source/android/libs/RmlUiDebugger.mk

include $(QFUSION_PATH)/source/angelwrap/Android.mk
include $(QFUSION_PATH)/source/cin/Android.mk
include $(QFUSION_PATH)/source/ftlib/Android.mk
include $(QFUSION_PATH)/source/ref_gl/Android.mk
include $(QFUSION_PATH)/source/snd_openal/Android.mk
include $(QFUSION_PATH)/source/snd_qf/Android.mk
include $(QFUSION_PATH)/source/ui/Android.mk

include $(QFUSION_PATH)/source/client/Android.mk

# Set NDK_APP_DST_DIR to the temporary folder where the compiled game DLLs should be located before creating a modules PK3 with them.
# On Windows, the absolute path is relative to the disk root.
# NDK_APP_DST_DIR := $(HOME)/gg.warsow/files/basewsw

include $(QFUSION_PATH)/source/cgame/Android.mk
include $(QFUSION_PATH)/source/game/Android.mk
