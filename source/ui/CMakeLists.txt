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

add_library(ui SHARED ${UI_SOURCES} ${UI_HEADERS})

target_include_directories(ui PRIVATE ${ANGELSCRIPT_INCLUDE_DIR} ${LIBROCKET_INCLUDE_DIR})
target_include_directories(ui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}) # for ui_precompiled.h

target_link_libraries(ui PRIVATE RmlCore RmlControls RmlDebugger)

set_target_properties(ui PROPERTIES COMPILE_DEFINITIONS "RMLUI_STATIC_LIB;RMLUI_NO_FONT_INTERFACE_DEFAULT;RMLUI_NO_THIRDPARTY_CONTAINERS;__STDC_LIMIT_MACROS;__STDC_CONSTANT_MACROS")
if (CMAKE_VERSION VERSION_GREATER 3.16.0)
    target_precompile_headers(ui PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/ui_precompiled.h>")
endif()

add_dependencies(ui angelwrap)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(NO_FONT_INTERFACE_DEFAULT "Do not build the font providers that come with the library" ON)
option(NO_THIRDPARTY_CONTAINERS "Only use standard library containers." ON)
add_subdirectory(${LIBROCKET_DIR} ${CMAKE_HOME_DIRECTORY}/build/ui/rmlui/)
set_property(TARGET RmlCore PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET RmlControls PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET RmlDebugger PROPERTY POSITION_INDEPENDENT_CODE ON)

qf_set_output_dir(ui libs)
