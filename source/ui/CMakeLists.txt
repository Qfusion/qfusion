project(ui)

include_directories(${LIBROCKET_INCLUDE_DIR})

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
	"../gameshared/q_*.cpp"
	"../qalgo/*.cpp"
	)

file(GLOB NANOSVG_HEADERS "${NANOSVG_INCLUDE_DIR}/*.h")
source_group("nanosvg\\Header Files" FILES ${NANOSVG_HEADERS})

include_directories(${ANGELSCRIPT_INCLUDE_DIR} ${LIBROCKET_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}) # for ui_precompiled.h

add_library(ui SHARED ${NANOSVG_HEADERS} ${UI_SOURCES} ${UI_HEADERS})
target_link_libraries(ui PRIVATE ${LIBROCKET_CORE_LIBRARY} ${LIBROCKET_CONTROLS_LIBRARY})
set_target_properties(ui PROPERTIES COMPILE_DEFINITIONS "ROCKET_STATIC_LIB;_SECURE_SCL=0; _USE_MATH_DEFINES")
qf_set_output_dir(ui libs)
set_property(TARGET ui PROPERTY CXX_STANDARD 11)
