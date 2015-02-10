# Configuration settings for shipping (full optimization)
CONFIG_NAME := Shipping
PLATFORM_NAME := Linux
PROJECT_NAME := OpenAL-MOB
TARGET_POSTFIX := 
    
PREPROCESSOR_MACROS := NDEBUG SHIPPING
INCLUDE_DIRS := ../include ../OpenAL32/Include ../mob/Include .
LIBRARY_DIRS :=
LIBRARY_NAMES :=

CFLAGS := -fPIC -msse2 -O3 -m32
LDFLAGS := -Wl,-gc-sections -m32
COMMONFLAGS :=

START_GROUP := -Wl,--start-group
END_GROUP := -Wl,--end-group

