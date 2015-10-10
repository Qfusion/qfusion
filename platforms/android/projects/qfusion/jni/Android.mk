QFUSION_PATH := $(call my-dir)

# Set this to the name of your game version header.
# QFUSION_APPLICATION_VERSION_HEADER := version.warsow.h

include $(NDK_ROOT)/sources/android/native_app_glue/Android.mk

include $(QFUSION_PATH)/source/android/angelscript.mk
include $(QFUSION_PATH)/source/android/curl.mk
include $(QFUSION_PATH)/source/android/freetype.mk
include $(QFUSION_PATH)/source/android/jpeg.mk
include $(QFUSION_PATH)/source/android/ogg.mk
include $(QFUSION_PATH)/source/android/OpenAL-MOB.mk
include $(QFUSION_PATH)/source/android/openssl-crypto.mk
include $(QFUSION_PATH)/source/android/openssl-ssl.mk
include $(QFUSION_PATH)/source/android/png.mk
include $(QFUSION_PATH)/source/android/theora.mk
include $(QFUSION_PATH)/source/android/vorbis.mk

include $(QFUSION_PATH)/libsrcs/libRocket/libRocket/build/android/Controls.mk
include $(QFUSION_PATH)/libsrcs/libRocket/libRocket/build/android/Core.mk

include $(QFUSION_PATH)/source/angelwrap/Android.mk
include $(QFUSION_PATH)/source/cin/Android.mk
include $(QFUSION_PATH)/source/ftlib/Android.mk
include $(QFUSION_PATH)/source/irc/Android.mk
include $(QFUSION_PATH)/source/ref_gl/Android.mk
include $(QFUSION_PATH)/source/snd_openal/Android.mk
include $(QFUSION_PATH)/source/snd_qf/Android.mk
include $(QFUSION_PATH)/source/ui/Android.mk

include $(QFUSION_PATH)/source/client/Android.mk

# Set NDK_APP_DST_DIR to the temporary folder where the compiled game DLLs should be located before creating a modules PK3 with them.
# On Windows, the absolute path is relative to the disk root.
# NDK_APP_DST_DIR := $(HOME)/net.warsow/files/basewsw

include $(QFUSION_PATH)/source/cgame/Android.mk
include $(QFUSION_PATH)/source/game/Android.mk
