project(ui)

file(GLOB UI_HEADERS
    "*.h"
    "kernel/*.h"
    "as/*.h"
    "datasources/*.h"
    "widgets/*.h"
    "decorators/*.h"
    "parsers/*.h"
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

include_directories(${ANGELSCRIPT_INCLUDE_DIR} ${LIBROCKET_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}) # for ui_precompiled.h

add_library(ui SHARED ${UI_SOURCES} ${UI_HEADERS})
target_link_libraries(ui ${LIBROCKET_LIBRARIES} ${FREETYPE_LIBRARIES})
set_target_properties(ui PROPERTIES COMPILE_DEFINITIONS "STATIC_LIB;_SECURE_SCL=0")
qf_set_output_dir(ui base)

# TODO: Remove this hack from here
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR MINGW)
    add_dependencies(ui ${LIBROCKET})
endif()