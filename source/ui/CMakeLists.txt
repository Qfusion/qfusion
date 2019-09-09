project(ui)

file(GLOB UI_HEADERS
    "*.h"
    "kernel/*.h"
    "as/*.h"
    "datasources/*.h"
    "widgets/*.h"
    "decorators/*.h"
    "parsers/*.h"
	"formatters/*.h"
	"../gameshared/q_*.h"
	"../qalgo/*.h"
)

file(GLOB UI_SOURCES
    "*.cpp"
    "kernel/*.cpp"
    "as/*.cpp"
    "datasources/*.cpp"
    "widgets/*.cpp"
    "decorators/*.cpp"
    "parsers/*.cpp"
    "../gameshared/q_*.c"
	"../qalgo/*.c"
)


file(GLOB NANOSVG_HEADERS
	"${NANOSVG_INCLUDE_DIR}/*.h"
)

source_group("nanosvg\\Header Files" FILES ${NANOSVG_HEADERS})

include_directories(${ANGELSCRIPT_INCLUDE_DIR} ${LIBROCKET_INCLUDE_DIR} ${NANOSVG_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}) # for ui_precompiled.h

add_library(ui SHARED ${NANOSVG_HEADERS} ${UI_SOURCES} ${UI_HEADERS})
target_link_libraries(ui PRIVATE RmlCore RmlControls RmlDebugger)
set_target_properties(ui PROPERTIES COMPILE_DEFINITIONS "RMLUI_STATIC_LIB;RMLUI_NO_FONT_INTERFACE_DEFAULT;RMLUI_NO_THIRDPARTY_CONTAINERS")
add_dependencies(ui angelwrap)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(NO_FONT_INTERFACE_DEFAULT "Do not build the font providers that come with the library" ON)
option(NO_THIRDPARTY_CONTAINERS "Only use standard library containers." ON)
add_subdirectory(${LIBROCKET_DIR} ${CMAKE_HOME_DIRECTORY}/build/ui/rmlui/)
set_property(TARGET RmlCore PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET RmlControls PROPERTY POSITION_INDEPENDENT_CODE ON)

qf_set_output_dir(ui libs)
