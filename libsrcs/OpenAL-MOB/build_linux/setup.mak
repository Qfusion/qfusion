# common makefile code

# make sure the user supplied everything
ifeq ($(PLATFORM_NAME),)
error:
	$(error Missing PLATFORM_NAME)
endif

ifeq ($(CONFIG_NAME),)
error:
	$(error Missing CONFIG_NAME)
endif

ifeq ($(PROJECT_NAME),)
error:
	$(error Missing PROJECT_NAME)
endif

# target directories
OBJDIR := tmp/$(CONFIG_NAME)
TARGETNAME := $(PROJECT_NAME)$(TARGET_POSTFIX).so
TARGETDIR := bin

# setup the full flags
CFLAGS += $(COMMONFLAGS) $(PLATFORM_CFLAGS) 
CFLAGS += $(addprefix -I,$(INCLUDE_DIRS)) 
CFLAGS += $(addprefix -D,$(PREPROCESSOR_MACROS))

ASFLAGS += $(COMMONFLAGS) $(PLATFORM_ASFLAGS) 
ASFLAGS += $(addprefix -D,$(PREPROCESSOR_MACROS))

LDFLAGS += $(COMMONFLAGS) 
LDFLAGS += $(PLATFORM_LDFLAGS) 
LDFLAGS += $(addprefix -L,$(LIBRARY_DIRS))
LDFLAGS += $(addprefix -l,$(LIBRARY_NAMES))
LDFLAGS += -Wl,-soname,$(TARGETNAME)

# targets
ALL_MAKE_FILES := Makefile $(CONFIG).mak setup.mak linux.mak
SOURCE_OBJECTS := $(SOURCEFILES:.c=.o)
ALL_OBJECTS := $(addprefix $(OBJDIR)/, $(notdir $(SOURCE_OBJECTS)))

# targets
all: $(TARGETDIR)/$(TARGETNAME)

$(TARGETDIR)/$(TARGETNAME): $(ALL_OBJECTS) $(EXTERNAL_LIBS) |$(TARGETDIR)
	$(LD) -shared -o $@ $(LDFLAGS) $(START_GROUP) $(ALL_OBJECTS) $(LDFLAGS) $(END_GROUP)

-include $(all_objs:.o=.dep)

clean:
	rm -rf $(OBJDIR)
	rm $(TARGETDIR)/$(TARGETNAME)

# rules to create the empty dirs
$(TARGETDIR):
	mkdir -p $(TARGETDIR)

$(OBJDIR):
	mkdir -p $(OBJDIR)

