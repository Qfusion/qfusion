LOCAL_PATH := $(call my-dir)/source
include $(CLEAR_VARS)
LOCAL_MODULE := ref_gl
LOCAL_MODULE_FILENAME := lib$(LOCAL_MODULE)_android_$(TARGET_ARCH_ABI)

LOCAL_LDLIBS := -landroid
LOCAL_STATIC_LIBRARIES := jpeg png

LOCAL_SRC_FILES := \
  android/android_glw.c \
  android/android_qgl.c \
  gameshared/q_math.c \
  gameshared/q_shared.c \
  qalgo/glob.c \
  qalgo/hash.c \
  qalgo/q_trie.c \
  qcommon/bsp.c \
  qcommon/patch.c \
  ref_gl/r_alias.c \
  ref_gl/r_backend.c \
  ref_gl/r_backend_program.c \
  ref_gl/r_cin.c \
  ref_gl/r_cmds.c \
  ref_gl/r_cull.c \
  ref_gl/r_framebuffer.c \
  ref_gl/r_image.c \
  ref_gl/r_imagelib.c \
  ref_gl/r_light.c \
  ref_gl/r_main.c \
  ref_gl/r_math.c \
  ref_gl/r_mesh.c \
  ref_gl/r_model.c \
  ref_gl/r_poly.c \
  ref_gl/r_portals.c \
  ref_gl/r_program.c \
  ref_gl/r_public.c \
  ref_gl/r_q3bsp.c \
  ref_gl/r_register.c \
  ref_gl/r_scene.c \
  ref_gl/r_shader.c \
  ref_gl/r_shadow.c \
  ref_gl/r_skin.c \
  ref_gl/r_skm.c \
  ref_gl/r_sky.c \
  ref_gl/r_surf.c \
  ref_gl/r_trace.c \
  ref_gl/r_vbo.c

include $(BUILD_SHARED_LIBRARY)
