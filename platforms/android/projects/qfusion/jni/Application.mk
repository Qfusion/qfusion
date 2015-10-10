APP_ABI := armeabi-v7a x86
APP_CFLAGS := -ffast-math -fno-strict-aliasing -funroll-loops -Werror=return-type
APP_CPPFLAGS := -fexceptions -frtti -std=c++0x
APP_OPTIM := release
APP_PLATFORM := android-16
APP_STL := gnustl_shared
NDK_APP_SHORT_COMMANDS := true

APP_MODULES := \
  android_native_app_glue \
  \
  angelscript \
  crypto \
  curl \
  freetype \
  jpeg \
  ogg \
  OpenAL-MOB \
  png \
  RocketCore \
  RocketControls \
  ssl \
  theora \
  vorbis \
  \
  angelwrap \
  cin \
  ftlib \
  irc \
  ref_gl \
  snd_openal \
  snd_qf \
  ui \
  \
  qfusion \
  \
  cgame \
  game
