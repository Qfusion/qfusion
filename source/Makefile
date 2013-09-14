###########################################################
# Variables
###########################################################

# do a debug build?
DEBUG_BUILD?=NO

# use mingw and build Windows binaries?
USE_MINGW?=NO

# client and dedicated server binaries
BUILD_CLIENT?=YES
BUILD_SERVER?=YES

# engine modules
BUILD_IRC?=YES
BUILD_SND_QF?=YES
BUILD_SND_OPENAL?=YES
BUILD_CIN?=YES
BUILD_FTLIB?=YES

# game modules
BUILD_CGAME?=YES
BUILD_GAME?=YES
BUILD_UI_ROCKET?=YES
BUILD_ANGELWRAP=YES

# Qfusion TV
BUILD_TV_SERVER?=YES

# Build Qfusion client without server capabilities?
BUILD_THIN_CLIENT?=NO

###########################################################

ifeq ($(USE_MINGW),YES)
ARCH?=x86
HOST?=i586-mingw32msvc

HOST_PREFIX=$(HOST)-

CC=$(HOST_PREFIX)gcc
CXX=$(HOST_PREFIX)g++
LD=$(HOST_PREFIX)gcc
LXX=$(HOST_PREFIX)g++
AR=$(HOST_PREFIX)ar
RES=$(HOST_PREFIX)windres
RANLIB=$(HOST_PREFIX)ranlib

SHARED_LIBRARY_EXTENSION=dll
else
CC?=gcc
CXX?=g++
LD=gcc
LXX?=g++
AR=ar
RANLIB=ranlib

# this nice line comes from the linux kernel makefile
BASE_ARCH:=$(shell uname -m | sed -e s/i.86/i386/ -e s/sun4u/sparc/ -e s/sparc64/sparc/ -e s/arm.*/arm/ -e s/sa110/arm/ -e s/alpha/axp/)
OS=$(shell uname)

ifeq ($(OS),FreeBSD)
ARCH?=freebsd_$(BASE_ARCH)
else
ARCH?=$(BASE_ARCH)
endif

SHARED_LIBRARY_EXTENSION=so
endif

BUILDDIR=build$(ARCH)

ifeq ($(DEBUG_BUILD), YES)
BINDIR=debug
CFLAGS_DEBUG=-O0 -ggdb -D_DEBUG
else
BINDIR=release
endif

DEPENDFILE_CLIENT=$(BUILDDIR)/client/Makefile.d
DEPENDFILE_OPENAL=$(BUILDDIR)/openal/Makefile.d
DEPENDFILE_QF=$(BUILDDIR)/qf/Makefile.d
DEPENDFILE_DED=$(BUILDDIR)/ded/Makefile.d
DEPENDFILE_CGAME=$(BUILDDIR)/cgame/Makefile.d
DEPENDFILE_GAME=$(BUILDDIR)/game/Makefile.d
DEPENDFILE_UI_ROCKET=$(BUILDDIR)/ui/Makefile.d
DEPENDFILE_IRC=$(BUILDDIR)/irc/Makefile.d
DEPENDFILE_CIN=$(BUILDDIR)/cin/Makefile.d
DEPENDFILE_FTLIB=$(BUILDDIR)/ftlib/Makefile.d
DEPENDFILE_ANGELWRAP=$(BUILDDIR)/angelwrap/Makefile.d
DEPENDFILE_TV_SERVER=$(BUILDDIR)/tv_server/Makefile.d
DEPENDFILE_NULL=$(BUILDDIR)/null/Makefile.d
DEPENDFILE_ROCKETCORE=$(BUILDDIR)/rocketcore/Makefile.d
DEPENDFILE_ROCKETCONTROLS=$(BUILDDIR)/rocketcontrols/Makefile.d

BUILDDIRS=$(BUILDDIR)/client $(BUILDDIR)/ded $(BUILDDIR)/cgame $(BUILDDIR)/game $(BUILDDIR)/ui $(BUILDDIR)/openal $(BUILDDIR)/qf $(BUILDDIR)/irc $(BUILDDIR)/cin $(BUILDDIR)/ftlib $(BUILDDIR)/angelwrap $(BUILDDIR)/tv_server $(BUILDDIR)/rocketcore $(BUILDDIR)/rocketcontrols

###########################################################
# Angelwrap stuff
ANGELSCRIPT_DIR=../libsrcs/angelscript/angelSVN/sdk/angelscript
ifeq ($(USE_MINGW),YES)
ifeq ($(CROSS_COMPILE),YES)
ANGELSCRIPT_PROJECT_DIR=$(ANGELSCRIPT_DIR)/projects/gnuc
else
ANGELSCRIPT_PROJECT_DIR=$(ANGELSCRIPT_DIR)/projects/mingw
endif
else
ANGELSCRIPT_PROJECT_DIR=$(ANGELSCRIPT_DIR)/projects/gnuc
endif
ANGELSCRIPT_LIB=$(ANGELSCRIPT_DIR)/lib/libangelscript.a
ANGELSCRIPT_INC=$(ANGELSCRIPT_DIR)/include/

CFLAGS_ANGELWRAP=-I$(ANGELSCRIPT_INC)

###########################################################
# Game modules stuff
CFLAGS_GAME=-I$(ANGELSCRIPT_INC)

###########################################################
# libRocket stuff
LIBROCKET_DIR=../libsrcs/libRocket/libRocket
LIBROCKET_MAKEFILE=Makefile.qfusion
LIBROCKET_INC=$(LIBROCKET_DIR)/Include
LIBROCKET_LIB=$(LIBROCKET_DIR)/lib/libRocketWSW.a

CFLAGS_ROCKET=-DSTATIC_LIB -I$(LIBROCKET_INC) -I$(ANGELSCRIPT_INC)
# add the base of ui to inclusions
CFLAGS_ROCKET+=-Iui

# dynamic link to 'official' build
# LDFLAGS_ROCKET=-L$(LIBROCKET_DIR)/Build -lRocketCore -lRocketControls -lfreetype

###########################################################

####################### MINGW ######################
ifeq ($(USE_MINGW),YES)
CFLAGS_COMMON=-D_M_IX86 $(CFLAGS) -MD -I. -Iwin32/include -I../libsrcs/zlib -I../libsrcs/libcurl/include -D__WIN32 -D__USE_LARGEFILE -D__USE_LARGEFILE64 -mwindows -Wall -Wno-unknown-pragmas -Wno-unused-function
CFLAGS_RELEASE=-O2 -fno-strict-aliasing -ffast-math -funroll-loops -DNDEBUG
CFLAGS_DEBUG=-O0 -ggdb -D_DEBUG
ifeq ($(DEBUG_BUILD),YES)
CFLAGS_COMMON+=$(CFLAGS_DEBUG)
else
CFLAGS_COMMON+=$(CFLAGS_RELEASE)
endif

CFLAGS_CLIENT=-I../libsrcs/libjpeg -I../libsrcs/libpng -I../dxsdk/include -DCURL_STATICLIB
CFLAGS_DED=-DDEDICATED_ONLY -DCURL_STATICLIB
CFLAGS_MODULE=-DPIC
CFLAGS_TV_SERVER=-DDEDICATED_ONLY -DTV_SERVER_ONLY -DTV_MODULE_HARD_LINKED -DCURL_STATICLIB

RESFLAGS_COMMON=-I. -Iwin32/include

LIBDIR=win32/$(ARCH)/lib/mingw

LDFLAGS_COMMON=-L$(LIBDIR) $(LDFLAGS) -lwinmm -Wl,-subsystem,windows
LXXFLAGS_COMMON=-L$(LIBDIR) $(LDFLAGS) -lwinmm -Wl,-subsystem,windows

ifeq ($(CROSS_COMPILE),YES)
LDFLAGS_COMMON+= -lgcc -static-libgcc
LXXFLAGS_COMMON+= -static-libgcc -static-libstdc++
endif

ifneq ($(DEBUG_BUILD),YES)
LDFLAGS_COMMON+= -s
LXXFLAGS_COMMON+= -s
endif

LDFLAGS_CLIENT=-lcurlstat -lwsock32 -ljpegstat -lpngstat -lgdi32 -L../dxsdk/lib/ -lddraw -ldinput -ldxguid -lws2_32 -lzstat
LDFLAGS_DED=-lcurlstat -lwsock32 -lws2_32 -lzstat
LDFLAGS_MODULE=-shared
LDFLAGS_TV_SERVER=-lcurlstat -lwsock32 -lws2_32 -lzstat

# static link to custombuilt lib
LDFLAGS_ROCKET=-L$(LIBROCKET_DIR)/lib -lRocketWSW -lfreetypetype

# openal
ifeq ($(BUILD_SND_OPENAL),YES)
CFLAGS_OPENAL=-I../libsrcs/libogg/include -I../libsrcs/libvorbis/include
LDFLAGS_OPENAL=-lvorbisfilestat -lvorbisstat -loggstat -L../dxsdk/lib/ -ldsound
endif

ifeq ($(BUILD_SND_QF),YES)
CFLAGS_QF=-I../libsrcs/libogg/include -I../libsrcs/libvorbis/include
CFLAGS_DSOUND=-I../dxsdk/include
LDFLAGS_QF=-lvorbisfilestat -lvorbisstat -loggstat
endif

ifeq ($(BUILD_CIN),YES)
CFLAGS_CIN=-I../libsrcs/libogg/include -I../libsrcs/libvorbis/include -I../libsrcs/libtheora/include
LDFLAGS_CIN=-lvorbisstat -loggstat -ltheorastat
endif

ifeq ($(BUILD_FTLIB),YES)
CFLAGS_FTLIB=-I../libsrcs/libfreetype/include
LDFLAGS_FTLIB=-lfreetypestat
endif

LDFLAGS_IRC=-lwsock32 -lws2_32
LDFLAGS_ANGELWRAP=$(ANGELSCRIPT_LIB)

CLIENT_EXE=qfusion_$(ARCH).exe
SERVER_EXE=qfusion_server_$(ARCH).exe
TV_SERVER_EXE=qfusiontv_server_$(ARCH).exe
else # USE_MINGW
####################### NORMAL *NIX? ######################
LOCALBASE?=/usr/local
X11BASE?=/usr/X11R6

CFLAGS_COMMON=$(CFLAGS) -pipe -I. -I$(LOCALBASE)/include -I$(X11BASE)/include -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -Wall -Wno-unused-function -fvisibility=hidden
CFLAGS_RELEASE=-O2 -fno-strict-aliasing -ffast-math -funroll-loops -DNDEBUG -msse2
CFLAGS_DEBUG=-O0 -ggdb -D_DEBUG
ifeq ($(DEBUG_BUILD),YES)
CFLAGS_COMMON+=$(CFLAGS_DEBUG)
else
CFLAGS_COMMON+=$(CFLAGS_RELEASE)
endif

CFLAGS_CLIENT=$(shell curl-config --cflags)
CFLAGS_DED=-DDEDICATED_ONLY -DC_ONLY $(shell curl-config --cflags)
CFLAGS_MODULE=-DPIC -fPIC
CFLAGS_TV_SERVER=-DDEDICATED_ONLY -DTV_SERVER_ONLY -DC_ONLY -DTV_MODULE_HARD_LINKED

LDFLAGS_COMMON=$(LDFLAGS)
LXXFLAGS_COMMON=$(LDFLAGS) -lstdc++ -lsupc++

ifeq ($(OS),FreeBSD)
LDFLAGS_COMMON+= -L/usr/local/lib -lm -pthread
else
LDFLAGS_COMMON+= -L/usr/local/lib -ldl -lm -O1 -Wl,--as-needed
endif

ifneq ($(DEBUG_BUILD),YES)
LDFLAGS_COMMON+= -s
LXXFLAGS_COMMON+= -s
endif

ifeq ($(ARCH),x86_64)
LIB=lib64
else
LIB=lib
endif

LDFLAGS_CLIENT=-ljpeg -lpng -lz -L$(X11BASE)/$(LIB) -lX11 -lXext -lXxf86dga -lXxf86vm -lXinerama -lXrandr -lrt $(shell curl-config --libs)
LDFLAGS_DED=-lz $(shell curl-config --libs)
LDFLAGS_MODULE=-shared
LDFLAGS_TV_SERVER=-lz $(shell curl-config --libs)

# static link to custombuilt lib
LDFLAGS_ROCKET=-L$(LIBROCKET_DIR)/lib -lRocketWSW -lfreetype

# openal
ifeq ($(BUILD_SND_OPENAL),YES)
OPENAL_CONFIG?=pkg-config openal

CFLAGS_OPENAL=$(shell $(OPENAL_CONFIG) --cflags)
LDFLAGS_OPENAL=-lvorbisfile
endif

# sdl
ifeq ($(BUILD_SND_QF),YES)
SDL_CONFIG?=sdl-config

CFLAGS_SDL=$(shell $(SDL_CONFIG) --cflags)
LDFLAGS_QF=$(shell $(SDL_CONFIG) --libs) -lvorbisfile
endif # BUILD_SND_QF

ifeq ($(BUILD_CIN),YES)
CFLAGS_CIN=
LDFLAGS_CIN=-lvorbis -ltheora -ltheoradec
endif

ifeq ($(BUILD_FTLIB),YES)
CFLAGS_FTLIB=$(shell freetype-config --cflags)
LDFLAGS_FTLIB=$(shell freetype-config --libs)
endif

LDFLAGS_IRC=
LDFLAGS_ANGELWRAP=$(ANGELSCRIPT_LIB) -lpthread

CLIENT_EXE=qfusion.$(ARCH)
SERVER_EXE=qfusion_server.$(ARCH)
TV_SERVER_EXE=qfusiontv_server.$(ARCH)
endif # USE_MINGW

####################### MAC support ######################
# Mac support, hacking this in at the end where it is easier than adding more if statements throughout
ifeq ($(OS),Darwin)
ARCH=mac
SHARED_LIBRARY_EXTENSION=dylib
LOCALBASE?=/usr
CFLAGS_COMMON=-arch ppc -arch i386 -I. -I$(LOCALBASE)/include -Wall -isysroot /Developer/SDKs/MacOSX10.4u.sdk -mmacosx-version-min=10.4
CFLAGS_RELEASE=-O2 -fno-strict-aliasing -ffast-math -funroll-loops -DNDEBUG -fvisibility=hidden
CFLAGS_DEBUG=-O0 -ggdb -D_DEBUG
ifeq ($(DEBUG_BUILD),YES)
CFLAGS_COMMON+=$(CFLAGS_DEBUG)
else
CFLAGS_COMMON+=$(CFLAGS_RELEASE)
endif
CFLAGS_OPENAL=-I/System/Library/Frameworks/OpenAL.framework/Headers
CFLAGS_QF=-I/Library/Frameworks/SDL.framework/Headers
CFLAGS_SDL=-I/Library/Frameworks/SDL.framework/Headers
CFLAGS_CLIENT=-std=gnu99 -I/Library/Frameworks/SDL.framework/Headers -I/Library/Frameworks/libjpeg.framework/Headers -I/System/Library/Frameworks/OpenAL.framework/Headers
CFLAGS_DED=-DDEDICATED_ONLY -DC_ONLY
CFLAGS_CIN=
CFLAGS_FTLIB=
LDFLAGS_CLIENT=-lz -lcurl -framework libjpeg -framework SDL -framework OpenGL -I/Library/Frameworks/SDL.framework/Headers
LDFLAGS_MODULE=-bundle
LDFLAGS_COMMON=-arch ppc -arch i386 -framework AppKit -mmacosx-version-min=10.4 -isysroot /Developer/SDKs/MacOSX10.4u.sdk
LXXFLAGS_COMMON=$(LDFLAGS_COMMON) -lstdc++ -lsupc++
LDFLAGS_DED=-lz -lcurl
LDFLAGS_QF=-framework SDL -framework Ogg -framework Vorbis
LDFLAGS_OPENAL=-framework OpenAL -framework Ogg -framework Vorbis
LDFLAGS_IRC=
LDFLAGS_ANGELWRAP=$(ANGELSCRIPT_LIB) -lpthread
LDFLAGS_CIN=
LDFLAGS_FTLIB=
endif

######################################################################################################
# Helper functions
######################################################################################################
DO_CC=echo "  > Compiling $<" && \
	$(CC) $(CFLAGS_COMMON) -c $< -o $@
DO_CXX=echo "  > Compiling (c++) $<" && \
	$(CXX) $(CFLAGS_COMMON) $(CXXFLAGS_COMMON) -c $< -o $@
DO_RES=echo "  > Compiling resource $<" && \
	$(RES) $(RESFLAGS_COMMON) -i $< -o $@

DO_CC_CLIENT=$(DO_CC) $(CFLAGS_CLIENT)
DO_CC_DED=$(DO_CC) $(CFLAGS_DED)
DO_CC_MODULE=$(DO_CC) $(CFLAGS_MODULE)
DO_CC_TV_SERVER=$(DO_CC) $(CFLAGS_TV_SERVER)

DO_CXX_MODULE=$(DO_CXX) $(CFLAGS_MODULE)

DO_RES_CLIENT=$(DO_RES)

######################################################################################################
# Files
######################################################################################################
#########
# CLIENT
#########
CFILES_CLIENT  = $(wildcard qcommon/*.c)
RESFILES_CLIENT =
ifeq ($(BUILD_THIN_CLIENT),YES)
CFILES_CLIENT += null/sv_null.c
else
CFILES_CLIENT += $(wildcard server/*.c)
endif
CFILES_CLIENT += $(wildcard client/*.c)
ifeq ($(USE_MINGW),YES)
CFILES_CLIENT += win32/win_vid.c win32/win_fs.c win32/win_qgl.c win32/win_net.c win32/conproc.c win32/win_glw.c win32/win_input.c win32/win_sys.c win32/win_lib.c
RESFILES_CLIENT += win32/qfusion.rc
else
ifeq ($(OS),Darwin)
CFILES_CLIENT += $(wildcard mac/*.c)
else
CFILES_CLIENT += unix/unix_input.c unix/unix_glw.c unix/unix_qgl.c unix_xpm.c
endif
CFILES_CLIENT += unix/unix_fs.c unix/unix_lib.c unix/unix_net.c unix/unix_sys.c unix/unix_vid.c
endif
CFILES_CLIENT += $(wildcard ref_gl/*.c)
CFILES_CLIENT += $(wildcard gameshared/q_*.c)
# CFILES_CLIENT += null/mm_null.c
CFILES_CLIENT += $(wildcard matchmaker/*.c)

CFILES_CLIENT_WITHOUT_PATH= $(notdir  $(CFILES_CLIENT))
RESFILES_CLIENT_WITHOUT_PATH= $(notdir  $(RESFILES_CLIENT))
OFILES_CLIENT=$(CFILES_CLIENT_WITHOUT_PATH:.c=.o)
OFILES_CLIENT+=$(RESFILES_CLIENT_WITHOUT_PATH:.rc=.o)
OBJS_CLIENT = $(addprefix $(BUILDDIR)/client/, $(OFILES_CLIENT) )

#########
# OPENAL
#########
CFILES_OPENAL  = $(wildcard snd_openal/*.c)
CFILES_OPENAL += $(wildcard gameshared/q_*.c)

CFILES_OPENAL_WITHOUT_PATH= $(notdir  $(CFILES_OPENAL))
OFILES_OPENAL=$(CFILES_OPENAL_WITHOUT_PATH:.c=.o)
OBJS_OPENAL = $(addprefix $(BUILDDIR)/openal/, $(OFILES_OPENAL) )

#########
# QF
#########
CFILES_QF  = $(wildcard snd_qf/*.c)
ifeq ($(USE_MINGW),YES)
CFILES_QF += win32/win_snd.c
else
CFILES_QF += unix/unix_snd.c
endif
CFILES_QF += $(wildcard gameshared/q_*.c)

CFILES_QF_WITHOUT_PATH= $(notdir  $(CFILES_QF))
OFILES_QF=$(CFILES_QF_WITHOUT_PATH:.c=.o)
OBJS_QF = $(addprefix $(BUILDDIR)/qf/, $(OFILES_QF) )

#########
# DED
#########
CFILES_DED  = qcommon/cm_main.c qcommon/cm_q3bsp.c qcommon/cm_trace.c qcommon/patch.c qcommon/common.c qcommon/glob.c qcommon/files.c qcommon/cmd.c qcommon/mem.c qcommon/net.c qcommon/net_chan.c qcommon/msg.c qcommon/cvar.c qcommon/md5.c qcommon/trie.c qcommon/dynvar.c qcommon/irc.c qcommon/library.c qcommon/mlist.c qcommon/webdownload.c qcommon/svnrev.c qcommon/snap_demos.c qcommon/snap_write.c qcommon/ascript.c qcommon/anticheat.c qcommon/wswcurl.c qcommon/cjson.c qcommon/base64.c
CFILES_DED += $(wildcard server/*.c)
CFILES_DED += null/cl_null.c
ifeq ($(USE_MINGW),YES)
CFILES_DED += win32/win_fs.c win32/win_net.c win32/conproc.c win32/win_sys.c win32/win_lib.c
else
CFILES_DED += unix/unix_fs.c unix/unix_net.c unix/unix_lib.c unix/unix_sys.c
endif
CFILES_DED += $(wildcard gameshared/q_*.c)
CFILES_DED += $(wildcard matchmaker/mm_*.c)

CFILES_DED_WITHOUT_PATH= $(notdir  $(CFILES_DED))
OFILES_DED=$(CFILES_DED_WITHOUT_PATH:.c=.o)
OBJS_DED = $(addprefix $(BUILDDIR)/ded/, $(OFILES_DED) )

#########
# CGAME
#########
CPPFILES_CGAME  = $(wildcard cgame/*.cpp)
CFILES_CGAME = $(wildcard gameshared/*.c)

CPPFILES_CGAME_WITHOUT_PATH= $(notdir  $(CPPFILES_CGAME))
OFILES_CGAME=$(CPPFILES_CGAME_WITHOUT_PATH:.cpp=.o)
CFILES_CGAME_WITHOUT_PATH= $(notdir  $(CFILES_CGAME))
OFILES_CGAME+=$(CFILES_CGAME_WITHOUT_PATH:.c=.o)
OBJS_CGAME = $(addprefix $(BUILDDIR)/cgame/, $(OFILES_CGAME) )

#########
# GAME
#########
CPPFILES_GAME  = $(wildcard game/*.cpp)
CPPFILES_GAME += $(wildcard game/ai/*.cpp)
CFILES_GAME = $(wildcard gameshared/*.c)
CFILES_GAME += matchmaker/mm_rating.c

CFILES_GAME_WITHOUT_PATH= $(notdir  $(CFILES_GAME))
OFILES_GAME=$(CFILES_GAME_WITHOUT_PATH:.c=.o)
CPPFILES_GAME_WITHOUT_PATH= $(notdir  $(CPPFILES_GAME))
OFILES_GAME+=$(CPPFILES_GAME_WITHOUT_PATH:.cpp=.o)
OBJS_GAME = $(addprefix $(BUILDDIR)/game/, $(OFILES_GAME) )

#########
# UI.ROCKET
#########
CPPFILES_UI_ROCKET = $(wildcard ui/*.cpp)
CPPFILES_UI_ROCKET += $(wildcard ui/kernel/*.cpp)
CPPFILES_UI_ROCKET += $(wildcard ui/as/*.cpp)
CPPFILES_UI_ROCKET += $(wildcard ui/datasources/*.cpp)
CPPFILES_UI_ROCKET += $(wildcard ui/widgets/*.cpp)
CPPFILES_UI_ROCKET += $(wildcard ui/decorators/*.cpp)
CFILES_UI_ROCKET = $(wildcard gameshared/q_*.c)

CFILES_UI_ROCKET_WITHOUT_PATH= $(notdir  $(CFILES_UI_ROCKET))
OFILES_UI_ROCKET=$(CFILES_UI_ROCKET_WITHOUT_PATH:.c=.o)
CPPFILES_UI_ROCKET_WITHOUT_PATH= $(notdir  $(CPPFILES_UI_ROCKET))
OFILES_UI_ROCKET+=$(CPPFILES_UI_ROCKET_WITHOUT_PATH:.cpp=.o)
OBJS_UI_ROCKET = $(addprefix $(BUILDDIR)/ui/, $(OFILES_UI_ROCKET) )

#########
# IRC
#########
CFILES_IRC  = $(wildcard irc/*.c)
CFILES_IRC += $(wildcard gameshared/q_*.c)

CFILES_IRC_WITHOUT_PATH= $(notdir  $(CFILES_IRC))
OFILES_IRC=$(CFILES_IRC_WITHOUT_PATH:.c=.o)
OBJS_IRC = $(addprefix $(BUILDDIR)/irc/, $(OFILES_IRC) )

#########
# CIN
#########
CFILES_CIN  = $(wildcard cin/*.c)
CFILES_CIN += $(wildcard gameshared/q_*.c)

CFILES_CIN_WITHOUT_PATH= $(notdir  $(CFILES_CIN))
OFILES_CIN=$(CFILES_CIN_WITHOUT_PATH:.c=.o)
OBJS_CIN = $(addprefix $(BUILDDIR)/cin/, $(OFILES_CIN) )

#########
# FTLIB
#########
CFILES_FTLIB  = $(wildcard ftlib/*.c)
CFILES_FTLIB += $(wildcard gameshared/q_*.c)

CFILES_FTLIB_WITHOUT_PATH= $(notdir  $(CFILES_FTLIB))
OFILES_FTLIB=$(CFILES_FTLIB_WITHOUT_PATH:.c=.o)
OBJS_FTLIB = $(addprefix $(BUILDDIR)/ftlib/, $(OFILES_FTLIB) )

#########
# ANGELWRAP
#########
CPPFILES_ANGELWRAP  = $(wildcard angelwrap/*.cpp)
CPPFILES_ANGELWRAP += $(wildcard angelwrap/addon/*.cpp)
CFILES_ANGELWRAP    = $(wildcard angelwrap/*.c)
CFILES_ANGELWRAP   += $(wildcard gameshared/q_*.c)

CFILES_ANGELWRAP_WITHOUT_PATH= $(notdir  $(CFILES_ANGELWRAP))
CPPFILES_ANGELWRAP_WITHOUT_PATH= $(notdir  $(CPPFILES_ANGELWRAP))
OFILES_ANGELWRAP=$(CFILES_ANGELWRAP_WITHOUT_PATH:.c=.o)
OFILES_ANGELWRAP+=$(CPPFILES_ANGELWRAP_WITHOUT_PATH:.cpp=.o)
OBJS_ANGELWRAP = $(addprefix $(BUILDDIR)/angelwrap/, $(OFILES_ANGELWRAP) )

#########
# TV SERVER
#########
CFILES_TV_SERVER  = qcommon/cm_main.c qcommon/cm_q3bsp.c qcommon/cm_trace.c qcommon/patch.c qcommon/common.c qcommon/glob.c qcommon/files.c qcommon/cmd.c qcommon/mem.c qcommon/net.c qcommon/net_chan.c qcommon/msg.c qcommon/cvar.c qcommon/md5.c qcommon/trie.c qcommon/dynvar.c qcommon/irc.c qcommon/library.c qcommon/svnrev.c qcommon/snap_demos.c qcommon/snap_read.c qcommon/snap_write.c qcommon/wswcurl.c
CFILES_TV_SERVER += $(wildcard tv_server/*.c)
CFILES_TV_SERVER += null/cl_null.c null/ascript_null.c null/mm_null.c
ifeq ($(USE_MINGW),YES)
CFILES_TV_SERVER += win32/win_fs.c win32/win_net.c win32/conproc.c win32/win_sys.c win32/win_lib.c
else
CFILES_TV_SERVER += unix/unix_fs.c unix/unix_net.c unix/unix_sys.c unix/unix_lib.c
endif
CFILES_TV_SERVER += $(wildcard gameshared/q_*.c)
CFILES_TV_SERVER += $(wildcard tv_server/tv_module/*.c)

CFILES_TV_SERVER_WITHOUT_PATH= $(notdir  $(CFILES_TV_SERVER))
OFILES_TV_SERVER=$(CFILES_TV_SERVER_WITHOUT_PATH:.c=.o)
OBJS_TV_SERVER = $(addprefix $(BUILDDIR)/tv_server/, $(OFILES_TV_SERVER) )

######################################################################################################
# Building Rules
######################################################################################################
.PHONY: all \
	client message-client compile-client link-client \
	openal message-openal compile-openal link-openal  \
	qf message-qf compile-qf link-qf  \
	ded message-ded compile-ded link-ded  \
	game message-game compile-game link-game  \
	cgame message-cgame compile-cgame link-cgame  \
	ui message-ui compile-ui link-ui \
	irc message-irc compile-irc link-irc \
	cin message-cin compile-cin link-cin \
	ftlib message-ftlib compile-ftlib link-ftlib \
	angelwrap message-angelwrap compile-angelwrap link-angelwrap \
	tv_server message-tv_server compile-tv_server link-tv_server  \
	clean clean-depend clean-client clean-openal clean-qf clean-ded \
	clean-cgame clean-game clean-irc clean-cin clean-angelwrap clean-game clean-tv_server \
	compile

all: client openal qf ded cgame game ui irc cin ftlib angelwrap tv_server

client: $(BUILDDIRS) message-client compile-client link-client start-script-client
openal: $(BUILDDIRS) message-openal compile-openal link-openal
qf: $(BUILDDIRS) message-qf compile-qf link-qf
ded: $(BUILDDIRS) message-ded compile-ded link-ded start-script-ded
cgame: $(BUILDDIRS) message-cgame compile-cgame link-cgame
game: $(BUILDDIRS) message-game compile-game link-game
ui: $(BUILDDIRS) message-ui compile-ui link-ui
irc: $(BUILDDIRS) message-irc compile-irc link-irc
cin: $(BUILDDIRS) message-cin compile-cin link-cin
ftlib: $(BUILDDIRS) message-ftlib compile-ftlib link-ftlib
angelwrap: $(BUILDDIRS) message-angelwrap compile-angelwrap link-angelwrap
tv_server: $(BUILDDIRS) message-tv_server compile-tv_server link-tv_server start-script-tv_server

clean: clean-msg clean-depend clean-client clean-openal clean-qf clean-ded clean-cgame clean-game clean-ui clean-librocket clean-irc clean-cin clean-ftlib clean-angelwrap clean-tv_server

clean-msg:
	@echo "> *********************************************************"
	@echo "> * Cleaning build files"
	@echo "> *********************************************************"
clean-depend:
	@echo "  > Removing depend files" && \
	$(RM) $(DEPENDFILE_CLIENT) $(DEPENDFILE_DED) $(DEPENDFILE_CGAME) $(DEPENDFILE_GAME) $(DEPENDFILE_UI_ROCKET) $(DEPENDFILE_OPENAL) $(DEPENDFILE_TV_SERVER) $(DEPENDFILE_QF) $(DEPENDFILE_IRC) $(DEPENDFILE_CIN) $(DEPENDFILE_FTLIB) $(DEPENDFILE_ANGELWRAP)

$(BUILDDIRS) $(BINDIR) $(BINDIR)/base $(BINDIR)/libs:
	@mkdir -p $@

ifeq ($(BUILD_CLIENT),YES)
message-client:
	@echo "> *********************************************************"
	@echo "> * Building client"
	@echo "> *********************************************************"
compile-client: $(OBJS_CLIENT)
link-client: $(BINDIR) $(BINDIR)/$(CLIENT_EXE)
start-script-client: $(BINDIR) $(BINDIR)/qfusion
clean-client:
	@echo "  > Removing client objects" && \
	$(RM) $(OBJS_CLIENT)
endif

ifeq ($(BUILD_SND_OPENAL),YES)
message-openal:
	@echo "> *********************************************************"
	@echo "> * Building openal"
	@echo "> *********************************************************"
compile-openal: $(OBJS_OPENAL)
link-openal: $(BINDIR)/libs $(BINDIR)/libs/snd_openal_$(ARCH).$(SHARED_LIBRARY_EXTENSION)
clean-openal:
	@echo "  > Removing openal objects" && \
	$(RM) $(OBJS_OPENAL)
endif

ifeq ($(BUILD_SND_QF),YES)
message-qf:
	@echo "> *********************************************************"
	@echo "> * Building qf"
	@echo "> *********************************************************"
compile-qf: $(OBJS_QF)
link-qf: $(BINDIR)/libs $(BINDIR)/libs/snd_qf_$(ARCH).$(SHARED_LIBRARY_EXTENSION)
clean-qf:
	@echo "  > Removing qf objects" && \
	$(RM) $(OBJS_QF)
endif

ifeq ($(BUILD_SERVER),YES)
message-ded:
	@echo "> *********************************************************"
	@echo "> * Building dedicated server"
	@echo "> *********************************************************"
compile-ded: $(OBJS_DED)
link-ded: $(BINDIR) $(BINDIR)/$(SERVER_EXE)
start-script-ded: $(BINDIR) $(BINDIR)/qfusion_server
clean-ded:
	@echo "  > Removing dedicated server objects" && \
	$(RM) $(OBJS_DED)
endif

ifeq ($(BUILD_CGAME),YES)
message-cgame:
	@echo "> *********************************************************"
	@echo "> * Building cgame"
	@echo "> *********************************************************"
compile-cgame: $(OBJS_CGAME)
link-cgame: $(BINDIR)/base $(BINDIR)/base/cgame_$(ARCH).$(SHARED_LIBRARY_EXTENSION)
clean-cgame:
	@echo "  > Removing cgame objects" && \
	$(RM) $(OBJS_CGAME)
endif

ifeq ($(BUILD_GAME),YES)
message-game:
	@echo "> *********************************************************"
	@echo "> * Building game"
	@echo "> *********************************************************"
compile-game: $(OBJS_GAME)
link-game: $(BINDIR)/base $(BINDIR)/base/game_$(ARCH).$(SHARED_LIBRARY_EXTENSION)
clean-game:
	@echo "  > Removing game objects" && \
	$(RM) $(OBJS_GAME)
endif

ifeq ($(BUILD_UI_ROCKET),YES)
message-ui:
	@echo "> *********************************************************"
	@echo "> * Building ui"
	@echo "> *********************************************************"
compile-ui: $(OBJS_UI_ROCKET) $(LIBROCKET_LIB)
link-ui: $(BINDIR)/base $(BINDIR)/base/ui_$(ARCH).$(SHARED_LIBRARY_EXTENSION)
clean-ui:
	@echo "  > Removing ui objects" && \
	$(RM) $(OBJS_UI_ROCKET)

$(LIBROCKET_LIB):
	@echo "> * Building Rocket library"
	@CXX=$(CXX) AR=$(AR) RANLIB=$(RANLIB) CXXFLAGS="$(CFLAGS)" DEBUG_BUILD=$(DEBUG_BUILD) USE_MINGW=$(USE_MINGW) $(MAKE) -C $(LIBROCKET_DIR) -f $(LIBROCKET_MAKEFILE) DEBUG_BUILD=$(DEBUG_BUILD)
	@echo "> * Done building RocketCore library."
	@echo "> *********************************************************"
	@echo "> * Continuing ui building..."

clean-librocket:
	@echo "  > Removing librocket objects" && \
	CXX=$(CXX) $(MAKE) -C $(LIBROCKET_DIR) -f $(LIBROCKET_MAKEFILE) clean
endif

ifeq ($(BUILD_IRC),YES)
message-irc:
	@echo "> *********************************************************"
	@echo "> * Building irc"
	@echo "> *********************************************************"
compile-irc: $(OBJS_IRC)
link-irc: $(BINDIR)/libs $(BINDIR)/libs/irc_$(ARCH).$(SHARED_LIBRARY_EXTENSION)
clean-irc:
	@echo "  > Removing irc objects" && \
	$(RM) $(OBJS_IRC)
endif

ifeq ($(BUILD_CIN),YES)
message-cin:
	@echo "> *********************************************************"
	@echo "> * Building cin"
	@echo "> *********************************************************"
compile-cin: $(OBJS_CIN)
link-cin: $(BINDIR)/libs $(BINDIR)/libs/cin_$(ARCH).$(SHARED_LIBRARY_EXTENSION)
clean-cin:
	@echo "  > Removing cin objects" && \
	$(RM) $(OBJS_CIN)
endif

ifeq ($(BUILD_FTLIB),YES)
message-ftlib:
	@echo "> *********************************************************"
	@echo "> * Building ftlib"
	@echo "> *********************************************************"
compile-ftlib: $(OBJS_FTLIB)
link-ftlib: $(BINDIR)/libs $(BINDIR)/libs/ftlib_$(ARCH).$(SHARED_LIBRARY_EXTENSION)
clean-ftlib:
	@echo "  > Removing ftlib objects" && \
	$(RM) $(OBJS_FTLIB)
endif

ifeq ($(BUILD_ANGELWRAP),YES)
message-angelwrap:
	@echo "> *********************************************************"
	@echo "> * Building angelwrap"
	@echo "> *********************************************************"
	-@rm $(ANGELSCRIPT_LIB)
$(ANGELSCRIPT_INC)/angelscript.h:
	@echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
	@echo "!!!"
	@echo "!!! ERROR: Angelscript SDK not found in '$(ANGELSCRIPT_DIR)' !!!"
	@echo "!!!        Try to update the SVN tree and retry"
	@echo "!!!"
	@echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
	@false

$(ANGELSCRIPT_LIB): $(ANGELSCRIPT_INC)/angelscript.h
	@echo "> * Building Angelscript library first"
	@CXX=$(CXX) AR=$(AR) RANLIB=$(RANLIB) CXXFLAGS="$(CFLAGS)" $(MAKE) -C $(ANGELSCRIPT_PROJECT_DIR)
	@echo "> * Done building angelscript library."
	@echo "> *********************************************************"
	@echo "> * Continuing angelwrap building..."

compile-angelwrap: $(ANGELSCRIPT_LIB) $(OBJS_ANGELWRAP)
link-angelwrap: $(BINDIR)/libs $(BINDIR)/libs/angelwrap_$(ARCH).$(SHARED_LIBRARY_EXTENSION)
clean-angelwrap:
	@echo "  > Removing angelscript objects"
	@CXX=$(CXX) $(MAKE) -C $(ANGELSCRIPT_PROJECT_DIR) clean
	@echo "  > Removing angelwrap objects" && \
	$(RM) $(OBJS_ANGELWRAP)
endif

ifeq ($(BUILD_TV_SERVER),YES)
message-tv_server:
	@echo "> *********************************************************"
	@echo "> * Building TV server"
	@echo "> *********************************************************"
compile-tv_server: $(OBJS_TV_SERVER)
link-tv_server: $(BINDIR) $(BINDIR)/$(TV_SERVER_EXE)
start-script-tv_server: $(BINDIR)/qfusiontv_server
clean-tv_server:
	@echo "  > Removing TV server objects" && \
	$(RM) $(OBJS_TV_SERVER)
endif

######################################################################################################
# Linking Rules
######################################################################################################
########
# DED
########
$(BINDIR)/$(SERVER_EXE): $(OBJS_DED)
	@echo "  > Linking $@" && \
	$(LD) -o $@ $^ $(LDFLAGS_COMMON) $(LDFLAGS_DED)

#########
# CLIENT
#########
ifeq ($(OS),Darwin)
$(BINDIR)/$(CLIENT_EXE): $(OBJS_CLIENT)
	@echo "  > Linking $@" && \
	$(LD) -o $@ $(OBJS_CLIENT) mac/SDLMain.m $(LDFLAGS_COMMON) $(LDFLAGS_CLIENT)
else
$(BINDIR)/$(CLIENT_EXE): $(OBJS_CLIENT)
	@echo "  > Linking $@" && \
	$(LD) -o $@ $(OBJS_CLIENT) $(LDFLAGS_COMMON) $(LDFLAGS_CLIENT)
endif

#########
# OPENAL
#########
$(BINDIR)/libs/snd_openal_$(ARCH).$(SHARED_LIBRARY_EXTENSION): $(OBJS_OPENAL)
	@echo "  > Linking $@" && \
	$(LD) -o $@ $^ $(LDFLAGS_COMMON) $(LDFLAGS_MODULE) $(LDFLAGS_OPENAL)

#########
# QF
#########
$(BINDIR)/libs/snd_qf_$(ARCH).$(SHARED_LIBRARY_EXTENSION): $(OBJS_QF)
	@echo "  > Linking $@" && \
	$(LD) -o $@ $^ $(LDFLAGS_COMMON) $(LDFLAGS_MODULE) $(LDFLAGS_QF)

########
# .SO
########
$(BINDIR)/base/cgame_$(ARCH).$(SHARED_LIBRARY_EXTENSION): $(OBJS_CGAME)
	@echo "  > Linking $@" && \
	$(LXX) -o $@ $^ $(LXXFLAGS_COMMON) $(LDFLAGS_MODULE)

$(BINDIR)/base/game_$(ARCH).$(SHARED_LIBRARY_EXTENSION): $(OBJS_GAME)
	@echo "  > Linking $@" && \
	$(LXX) -o $@ $^ $(LXXFLAGS_COMMON) $(LDFLAGS_MODULE)

$(BINDIR)/base/ui_$(ARCH).$(SHARED_LIBRARY_EXTENSION): $(OBJS_UI_ROCKET)
	@echo "  > Linking $@" && \
	$(LXX) -o $@ $^ $(LXXFLAGS_COMMON) $(LDFLAGS_MODULE) $(LDFLAGS_ROCKET)

$(BINDIR)/libs/irc_$(ARCH).$(SHARED_LIBRARY_EXTENSION): $(OBJS_IRC)
	@echo "  > Linking $@" && \
	$(LD) -o $@ $^ $(LDFLAGS_COMMON) $(LDFLAGS_MODULE) $(LDFLAGS_IRC)

$(BINDIR)/libs/cin_$(ARCH).$(SHARED_LIBRARY_EXTENSION): $(OBJS_CIN)
	@echo "  > Linking $@" && \
	$(LD) -o $@ $^ $(LDFLAGS_COMMON) $(LDFLAGS_MODULE) $(LDFLAGS_CIN)

$(BINDIR)/libs/ftlib_$(ARCH).$(SHARED_LIBRARY_EXTENSION): $(OBJS_FTLIB)
	@echo "  > Linking $@" && \
	$(LD) -o $@ $^ $(LDFLAGS_COMMON) $(LDFLAGS_MODULE) $(LDFLAGS_FTLIB)

$(BINDIR)/libs/angelwrap_$(ARCH).$(SHARED_LIBRARY_EXTENSION): $(OBJS_ANGELWRAP)
	@echo "  > Linking $@" && \
	$(LXX) -o $@ $^ $(LXXFLAGS_COMMON) $(LDFLAGS_MODULE) $(LDFLAGS_ANGELWRAP)

########
# TV
########
$(BINDIR)/$(TV_SERVER_EXE): $(OBJS_TV_SERVER)
	@echo "  > Linking $@" && \
	$(LD) -o $@ $^ $(LDFLAGS_COMMON) $(LDFLAGS_TV_SERVER)

######################################################################################################
# Meta Rules
######################################################################################################

unix/start_script.sh:

########
# CLIENT
########
$(BUILDDIR)/client/%.o: qcommon/%.c
	@$(DO_CC_CLIENT)

$(BUILDDIR)/client/%.o: server/%.c
	@$(DO_CC_CLIENT)

$(BUILDDIR)/client/%.o: client/%.c
	@$(DO_CC_CLIENT)

$(BUILDDIR)/client/%.o: ref_gl/%.c
	@$(DO_CC_CLIENT)

$(BUILDDIR)/client/%.o: null/%.c
	@$(DO_CC_CLIENT)

$(BUILDDIR)/client/%.o: matchmaker/%.c
	@$(DO_CC_CLIENT)

ifeq ($(USE_MINGW),YES)
$(BUILDDIR)/client/%.o: win32/%.c
	@$(DO_CC_CLIENT)

$(BUILDDIR)/client/%.o: win32/%.rc
	@$(DO_RES_CLIENT)
else
ifeq ($(OS),Darwin)
$(BUILDDIR)/client/%.o: mac/%.c
	@$(DO_CC_CLIENT)
$(BUILDDIR)/client/%.o: unix/%.c
	@$(DO_CC_CLIENT)
else
$(BUILDDIR)/client/%.o: unix/%.c
	@$(DO_CC_CLIENT)
endif
endif

$(BUILDDIR)/client/%.o: game/%.c
	@$(DO_CC_CLIENT)

$(BUILDDIR)/client/%.o: gameshared/%.c
	@$(DO_CC_DED)

$(BINDIR)/qfusion: unix/start_script.sh
	@echo "  > Installing client start script" && \
	cp $< $@ && \
	chmod a+x $@

########
# OPENAL
########
$(BUILDDIR)/openal/%.o: snd_openal/%.c
	@$(DO_CC_MODULE) $(CFLAGS_OPENAL)

$(BUILDDIR)/openal/%.o: gameshared/%.c
	@$(DO_CC_MODULE)

########
# QF
########
$(BUILDDIR)/qf/%.o: snd_qf/%.c
	@$(DO_CC_MODULE) $(CFLAGS_QF)

ifeq ($(USE_MINGW),YES)
$(BUILDDIR)/qf/win_snd.o: win32/win_snd.c
	@$(DO_CC_MODULE) $(CFLAGS_DSOUND)
else
$(BUILDDIR)/qf/unix_snd.o: unix/unix_snd.c
	@$(DO_CC_MODULE) $(CFLAGS_SDL)
endif

$(BUILDDIR)/qf/%.o: gameshared/%.c
	@$(DO_CC_MODULE)


########
# DED
########
$(BUILDDIR)/ded/%.o: qcommon/%.c
	@$(DO_CC_DED)

$(BUILDDIR)/ded/%.o: server/%.c
	@$(DO_CC_DED)

ifeq ($(USE_MINGW),YES)
$(BUILDDIR)/ded/%.o: win32/%.c
	@$(DO_CC_DED)
else
$(BUILDDIR)/ded/%.o: unix/%.c
	@$(DO_CC_DED)
endif

$(BUILDDIR)/ded/%.o: null/%.c
	@$(DO_CC_DED)

$(BUILDDIR)/ded/%.o: game/%.c
	@$(DO_CC_DED)

$(BUILDDIR)/ded/%.o: gameshared/%.c
	@$(DO_CC_DED)

$(BUILDDIR)/ded/%.o: matchmaker/%.c
	@$(DO_CC_DED)

$(BINDIR)/qfusion_server: unix/start_script.sh
	@echo "  > Installing dedicated server start script" && \
	cp $< $@ && \
	chmod a+x $@

########
# CGAME
########
$(BUILDDIR)/cgame/%.o: cgame/%.cpp
	@$(DO_CXX_MODULE)

$(BUILDDIR)/cgame/%.o: gameshared/%.c
	@$(DO_CC_MODULE)

########
# GAME
########
$(BUILDDIR)/game/%.o: game/%.cpp
	@$(DO_CXX_MODULE) $(CFLAGS_GAME)

$(BUILDDIR)/game/%.o: game/ai/%.cpp
	@$(DO_CXX_MODULE) $(CFLAGS_GAME)

$(BUILDDIR)/game/%.o: gameshared/%.c
	@$(DO_CC_MODULE) $(CFLAGS_GAME)

$(BUILDDIR)/game/%.o: matchmaker/%.c
	@$(DO_CC_MODULE) $(CFLAGS_GAME)

########
# UI.ROCKET
########
$(BUILDDIR)/ui/%.o: gameshared/%.c
	@$(DO_CC_MODULE)

$(BUILDDIR)/ui/%.o: ui/%.cpp
	@$(DO_CXX_MODULE) $(CFLAGS_ROCKET)

$(BUILDDIR)/ui/%.o: ui/kernel/%.cpp
	@$(DO_CXX_MODULE) $(CFLAGS_ROCKET)

$(BUILDDIR)/ui/%.o: ui/as/%.cpp
	@$(DO_CXX_MODULE) $(CFLAGS_ROCKET)

$(BUILDDIR)/ui/%.o: ui/datasources/%.cpp
	@$(DO_CXX_MODULE) $(CFLAGS_ROCKET)

$(BUILDDIR)/ui/%.o: ui/widgets/%.cpp
	@$(DO_CXX_MODULE) $(CFLAGS_ROCKET)

$(BUILDDIR)/ui/%.o: ui/decorators/%.cpp
	@$(DO_CXX_MODULE) $(CFLAGS_ROCKET)

#$(BUILDDIR)/ui.as/%.o: matchmaker/%.c
#	@$(DO_CC_MODULE)

########
# IRC
########
$(BUILDDIR)/irc/%.o: gameshared/%.c
	@$(DO_CC_MODULE)

$(BUILDDIR)/irc/%.o: irc/%.c
	@$(DO_CC_MODULE)

########
# CIN
########
$(BUILDDIR)/cin/%.o: gameshared/%.c
	@$(DO_CC_MODULE)

$(BUILDDIR)/cin/%.o: cin/%.c
	@$(DO_CC_MODULE) $(CFLAGS_CIN)

########
# FTLIB
########
$(BUILDDIR)/ftlib/%.o: gameshared/%.c
	@$(DO_CC_MODULE)

$(BUILDDIR)/ftlib/%.o: ftlib/%.c
	@$(DO_CC_MODULE) $(CFLAGS_FTLIB)

########
# ANGELWRAP
########
$(BUILDDIR)/angelwrap/%.o: gameshared/%.c
	@$(DO_CC_MODULE) $(CFLAGS_ANGELWRAP)

$(BUILDDIR)/angelwrap/%.o: angelwrap/%.c
	@$(DO_CC_MODULE) $(CFLAGS_ANGELWRAP)

$(BUILDDIR)/angelwrap/%.o: angelwrap/%.cpp
	@$(DO_CXX_MODULE) $(CFLAGS_ANGELWRAP)

$(BUILDDIR)/angelwrap/%.o : angelwrap/addon/%.cpp
	@$(DO_CXX_MODULE) $(CFLAGS_ANGELWRAP)

########
# TV SERVER
########
$(BUILDDIR)/tv_server/%.o: qcommon/%.c
	@$(DO_CC_TV_SERVER)

$(BUILDDIR)/tv_server/%.o: client/%.c
	@$(DO_CC_TV_SERVER)

$(BUILDDIR)/tv_server/%.o: tv_server/tv_module/%.c
	@$(DO_CC_TV_SERVER)

$(BUILDDIR)/tv_server/%.o: tv_server/%.c
	@$(DO_CC_TV_SERVER)

ifeq ($(USE_MINGW),YES)
$(BUILDDIR)/tv_server/%.o: win32/%.c
	@$(DO_CC_TV_SERVER)
else
$(BUILDDIR)/tv_server/%.o: unix/%.c
	@$(DO_CC_TV_SERVER)
endif

$(BUILDDIR)/tv_server/%.o: null/%.c
	@$(DO_CC_TV_SERVER)

$(BUILDDIR)/tv_server/%.o: gameshared/%.c
	@$(DO_CC_TV_SERVER)

$(BINDIR)/qfusiontv_server: unix/start_script.sh
	@echo "  > Installing TV server start script" && \
	cp $< $@ && \
	chmod a+x $@

######################################################################################################
# Depends
######################################################################################################

$(DEPENDFILE_CLIENT): $(BUILDDIRS) $(CFILES_CLIENT)
	@makedepend -f - -- $(CFLAGS_COMMON) -- $(CFILES_CLIENT) 2> /dev/null | \
	awk '/^[a-zA-Z_]/{ sub(/^([a-zA-Z_]*\/)+/, "$(BUILDDIR)/client/"); print $$0, "Makefile"; \
		sub(/^.*\:/, "$(DEPENDFILE_CLIENT):"); print $$0, "Makefile" }' > $(DEPENDFILE_CLIENT)

$(DEPENDFILE_OPENAL): $(BUILDDIRS) $(CFILES_OPENAL)
	@makedepend -f - -- $(CFLAGS_COMMON) $(CFLAGS_MODULES) $(CFLAGS_OPENAL) -- $(CFILES_OPENAL) 2> /dev/null | \
	awk '/^[a-zA-Z_]/{ sub(/^([a-zA-Z_]*\/)+/, "$(BUILDDIR)/openal/"); print $$0, "Makefile"; \
		sub(/^.*\:/, "$(DEPENDFILE_OPENAL):"); print $$0, "Makefile" }' > $(DEPENDFILE_OPENAL)

ifeq ($(USE_MINGW),YES)
$(DEPENDFILE_QF): $(BUILDDIRS) $(CFILES_QF)
	@makedepend -f - -- $(CFLAGS_COMMON) $(CFLAGS_MODULES) -- $(CFILES_QF) 2> /dev/null | \
	awk '/^[a-zA-Z_]/{ sub(/^([a-zA-Z_]*\/)+/, "$(BUILDDIR)/qf/"); print $$0, "Makefile"; \
		sub(/^.*\:/, "$(DEPENDFILE_QF):"); print $$0, "Makefile" }' > $(DEPENDFILE_QF)
else
$(DEPENDFILE_QF): $(BUILDDIRS) $(CFILES_QF)
	@makedepend -f - -- $(CFLAGS_COMMON) $(CFLAGS_MODULES) $(CFLAGS_SDL) -- $(CFILES_QF) 2> /dev/null | \
	awk '/^[a-zA-Z_]/{ sub(/^([a-zA-Z_]*\/)+/, "$(BUILDDIR)/qf/"); print $$0, "Makefile"; \
		sub(/^.*\:/, "$(DEPENDFILE_QF):"); print $$0, "Makefile" }' > $(DEPENDFILE_QF)
endif

$(DEPENDFILE_DED): $(BUILDDIRS) $(CFILES_DED)
	@makedepend -f - -- $(CFLAGS_COMMON) $(CFLAGS_DED) -- $(CFILES_DED) 2> /dev/null | \
	awk '/^[a-zA-Z_]/{ sub(/^([a-zA-Z_]*\/)+/, "$(BUILDDIR)/ded/"); print $$0, "Makefile"; \
		sub(/^.*\:/, "$(DEPENDFILE_DED):"); print $$0, "Makefile" }' > $(DEPENDFILE_DED)

$(DEPENDFILE_CGAME): $(BUILDDIRS) $(CFILES_CGAME)
	@makedepend -f - -- $(CFLAGS_COMMON) $(CFLAGS_MODULES) -- $(CFILES_CGAME) 2> /dev/null | \
	awk '/^[a-zA-Z_]/{ sub(/^([a-zA-Z_]*\/)+/, "$(BUILDDIR)/cgame/"); print $$0, "Makefile"; \
		sub(/^.*\:/, "$(DEPENDFILE_CGAME):"); print $$0, "Makefile" }' > $(DEPENDFILE_CGAME)

$(DEPENDFILE_GAME): $(BUILDDIRS) $(CFILES_GAME)
	@makedepend -f - -- $(CFLAGS_COMMON) $(CFLAGS_MODULES) -- $(CFILES_GAME) 2> /dev/null | \
	awk '/^[a-zA-Z_]/{ sub(/^([a-zA-Z_]*\/)+/, "$(BUILDDIR)/game/"); print $$0, "Makefile"; \
		sub(/^.*\:/, "$(DEPENDFILE_GAME):"); print $$0, "Makefile" }' > $(DEPENDFILE_GAME)

$(DEPENDFILE_UI_ROCKET): $(BUILDDIRS) $(CFILES_UI_ROCKET) $(CPPFILES_UI_ROCKET)
	@makedepend -f - -- $(CFLAGS_COMMON) $(CFLAGS_MODULES) -- $(CFILES_UI_ROCKET) $(CPPFILES_UI_ROCKET) 2> /dev/null | \
	awk '/^[a-zA-Z_]/{ sub(/^([a-zA-Z_]*\/)+/, "$(BUILDDIR)/ui/"); print $$0, "Makefile"; \
		sub(/^.*\:/, "$(DEPENDFILE_UI_ROCKET):"); print $$0, "Makefile" }' > $(DEPENDFILE_UI_ROCKET)

$(DEPENDFILE_IRC): $(BUILDDIRS) $(CFILES_IRC)
	@makedepend -f - -- $(CFLAGS_COMMON) $(CFLAGS_MODULES) -- $(CFILES_IRC) 2> /dev/null | \
	awk '/^[a-zA-Z_]/{ sub(/^([a-zA-Z_]*\/)+/, "$(BUILDDIR)/irc/"); print $$0, "Makefile"; \
		sub(/^.*\:/, "$(DEPENDFILE_IRC):"); print $$0, "Makefile" }' > $(DEPENDFILE_IRC)

$(DEPENDFILE_CIN): $(BUILDDIRS) $(CFILES_CIN)
	@makedepend -f - -- $(CFLAGS_COMMON) $(CFLAGS_MODULES) -- $(CFILES_CIN) 2> /dev/null | \
	awk '/^[a-zA-Z_]/{ sub(/^([a-zA-Z_]*\/)+/, "$(BUILDDIR)/cin/"); print $$0, "Makefile"; \
		sub(/^.*\:/, "$(DEPENDFILE_CIN):"); print $$0, "Makefile" }' > $(DEPENDFILE_CIN)

$(DEPENDFILE_FTLIB): $(BUILDDIRS) $(CFILES_FTLIB)
	@makedepend -f - -- $(CFLAGS_COMMON) $(CFLAGS_MODULES) -- $(CFILES_FTLIB) 2> /dev/null | \
	awk '/^[a-zA-Z_]/{ sub(/^([a-zA-Z_]*\/)+/, "$(BUILDDIR)/ftlib/"); print $$0, "Makefile"; \
		sub(/^.*\:/, "$(DEPENDFILE_FTLIB):"); print $$0, "Makefile" }' > $(DEPENDFILE_FTLIB)

$(DEPENDFILE_ANGELWRAP): $(BUILDDIRS) $(CFILES_ANGELWRAP)
	@makedepend -f - -- $(CFLAGS_COMMON) $(CFLAGS_MODULES) -- $(CFILES_ANGELWRAP) 2> /dev/null | \
	awk '/^[a-zA-Z_]/{ sub(/^([a-zA-Z_]*\/)+/, "$(BUILDDIR)/angelwrap/"); print $$0, "Makefile"; \
		sub(/^.*\:/, "$(DEPENDFILE_ANGELWRAP):"); print $$0, "Makefile" }' > $(DEPENDFILE_ANGELWRAP)

$(DEPENDFILE_TV_SERVER): $(BUILDDIRS) $(CFILES_TV_SERVER)
	@makedepend -f - -- $(CFLAGS_COMMON) $(CFLAGS_TV_SERVER) -- $(CFILES_TV_SERVER) 2> /dev/null | \
	awk '/^[a-zA-Z_]/{ sub(/^([a-zA-Z_]*\/)+/, "$(BUILDDIR)/tv_server/"); print $$0, "Makefile"; \
		sub(/^.*\:/, "$(DEPENDFILE_TV_SERVER):"); print $$0, "Makefile" }' > $(DEPENDFILE_TV_SERVER)

ifeq ($(BUILD_CLIENT),YES)
-include $(DEPENDFILE_CLIENT)
endif
ifeq ($(BUILD_SND_OPENAL),YES)
-include $(DEPENDFILE_OPENAL)
endif
ifeq ($(BUILD_SND_QF),YES)
-include $(DEPENDFILE_QF)
endif
ifeq ($(BUILD_SERVER),YES)
-include $(DEPENDFILE_DED)
endif
ifeq ($(BUILD_CGAME),YES)
-include $(DEPENDFILE_CGAME)
endif
ifeq ($(BUILD_GAME),YES)
-include $(DEPENDFILE_GAME)
endif
ifeq ($(BUILD_UI_ROCKET),YES)
-include $(DEPENDFILE_UI_ROCKET)
endif
ifeq ($(BUILD_IRC),YES)
-include $(DEPENDFILE_IRC)
endif
ifeq ($(BUILD_CIN),YES)
-include $(DEPENDFILE_CIN)
endif
ifeq ($(BUILD_FTLIB),YES)
-include $(DEPENDFILE_FTLIB)
endif
ifeq ($(BUILD_ANGELWRAP),YES)
-include $(DEPENDFILE_TV_SERVER)
endif
ifeq ($(BUILD_TV_SERVER),YES)
-include $(DEPENDFILE_TV_SERVER)
endif
