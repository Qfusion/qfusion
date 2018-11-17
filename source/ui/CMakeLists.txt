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

file(GLOB ROCKET_ROOT_HEADERS
	"${LIBROCKET_INCLUDE_DIR}/Rocket/*.h"
)
file(GLOB ROCKET_CORE_HEADERS
	"${LIBROCKET_INCLUDE_DIR}/Rocket/Core/*.h"
)
file(GLOB ROCKET_CONTROLS_HEADERS
	"${LIBROCKET_INCLUDE_DIR}/Rocket/Controls/*.h"
)
file(GLOB ROCKET_CORE_SOURCES
	"${LIBROCKET_SOURCE_DIR}/Core/*.cpp"
	"${LIBROCKET_SOURCE_DIR}/Core/*.inl"
)
file(GLOB ROCKET_CONTROLS_SOURCES
	"${LIBROCKET_SOURCE_DIR}/Controls/*.cpp"
	"${LIBROCKET_SOURCE_DIR}/Controls/*.inl"
)

file(GLOB NANOSVG_HEADERS
	"${NANOSVG_INCLUDE_DIR}/*.h"
)

source_group("Rocket\\Header Files" FILES ${ROCKET_ROOT_HEADERS})
source_group("Rocket\\Header Files\\Core" FILES ${ROCKET_CORE_HEADERS})
source_group("Rocket\\Header Files\\Controls" FILES ${ROCKET_CONTROLS_HEADERS})
source_group("Rocket\\Source Files\\Core" FILES ${ROCKET_CORE_SOURCES})
source_group("Rocket\\Source Files\\Controls" FILES ${ROCKET_CONTROLS_SOURCES})

source_group("nanosvg\\Header Files" FILES ${NANOSVG_HEADERS})

include_directories(${ANGELSCRIPT_INCLUDE_DIR} ${LIBROCKET_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}) # for ui_precompiled.h

add_library(ui SHARED ${ROCKET_ROOT_HEADERS} ${ROCKET_CORE_HEADERS} ${ROCKET_CONTROLS_HEADERS} ${ROCKET_CORE_SOURCES} ${ROCKET_CONTROLS_SOURCES} ${NANOSVG_HEADERS} ${UI_SOURCES} ${UI_HEADERS})
target_link_libraries(ui PRIVATE)
add_dependencies(ui angelwrap)
set_target_properties(ui PROPERTIES COMPILE_DEFINITIONS "ROCKET_STATIC_LIB;_SECURE_SCL=0; _USE_MATH_DEFINES")
qf_set_output_dir(ui libs)
set_property(TARGET ui PROPERTY CXX_STANDARD 11)
