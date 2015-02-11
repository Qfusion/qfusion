LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := ref_gl
LOCAL_MODULE_FILENAME := lib$(LOCAL_MODULE)_android_$(TARGET_ARCH_ABI)

LOCAL_LDLIBS := -landroid
LOCAL_STATIC_LIBRARIES := jpeg png

LOCAL_SRC_FILES := \
  ../android/android_glw.c \
  ../android/android_qgl.c \
  ../gameshared/q_math.c \
  ../gameshared/q_shared.c \
  ../qalgo/glob.c \
  ../qalgo/hash.c \
  ../qalgo/q_trie.c \
  ../qcommon/bsp.c \
  ../qcommon/patch.c \
  r_alias.c \
  r_backend.c \
  r_backend_program.c \
  r_cin.c \
  r_cmds.c \
  r_cull.c \
  r_framebuffer.c \
  r_image.c \
  r_imagelib.c \
  r_light.c \
  r_main.c \
  r_math.c \
  r_mesh.c \
  r_model.c \
  r_poly.c \
  r_portals.c \
  r_program.c \
  r_public.c \
  r_q3bsp.c \
  r_register.c \
  r_scene.c \
  r_shader.c \
  r_shadow.c \
  r_skin.c \
  r_skm.c \
  r_sky.c \
  r_surf.c \
  r_trace.c \
  r_vbo.c

include $(BUILD_SHARED_LIBRARY)
