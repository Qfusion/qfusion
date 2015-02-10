# Configuration settings for debug
CONFIG_NAME := Debug
PLATFORM_NAME := Linux
PROJECT_NAME := OpenAL-MOB
TARGET_POSTFIX := Debug
    
PREPROCESSOR_MACROS := DEBUG _DEBUG
INCLUDE_DIRS := ../include ../OpenAL32/Include ../mob/Include .
LIBRARY_DIRS :=
LIBRARY_NAMES :=

CFLAGS := -ggdb -fPIC -msse2 -O0 -m32
CXXFLAGS := $(CFLAGS)
LDFLAGS :=  -m32 -Wl,-gc-sections
COMMONFLAGS :=

START_GROUP := -Wl,--start-group
END_GROUP := -Wl,--end-group

