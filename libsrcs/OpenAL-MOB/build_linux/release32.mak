# Configuration settings for release
CONFIG_NAME := Release
PLATFORM_NAME := Linux
PROJECT_NAME := OpenAL-MOB
TARGET_POSTFIX := Release
    
PREPROCESSOR_MACROS := NDEBUG
INCLUDE_DIRS := ../include ../OpenAL32/Include ../mob/Include .
LIBRARY_DIRS :=
LIBRARY_NAMES :=

CFLAGS := -ggdb -fPIC -msse2 -O2 -m32
LDFLAGS := -Wl,-gc-sections  -m32
COMMONFLAGS :=

START_GROUP := -Wl,--start-group
END_GROUP := -Wl,--end-group

