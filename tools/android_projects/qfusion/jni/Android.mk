QFUSION_PATH := $(call my-dir)

include $(QFUSION_PATH)/angelscript.mk
include $(QFUSION_PATH)/freetype.mk
include $(QFUSION_PATH)/jpeg.mk
include $(QFUSION_PATH)/ogg.mk
include $(QFUSION_PATH)/png.mk
include $(QFUSION_PATH)/vorbis.mk

include $(QFUSION_PATH)/angelwrap.mk
include $(QFUSION_PATH)/ftlib.mk
include $(QFUSION_PATH)/irc.mk
include $(QFUSION_PATH)/ref_gl.mk
