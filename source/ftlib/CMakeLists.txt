project(ftlib)

include_directories(${CURL_INCLUDE_DIR} ${ZLIB_INCLUDE_DIR} ${FREETYPE_INCLUDE_DIRS})

file(GLOB FTLIB_HEADERS
    "*.h"
	"../gameshared/q_*.h"
)

file(GLOB FTLIB_SOURCES
    "*.c"
    "../gameshared/q_*.c"
)

if (UNIX AND NOT APPLE)
        set(FTLIB_PLATFORM_LIBRARIES m)
endif()

add_library(ftlib SHARED ${FTLIB_SOURCES} ${FTLIB_HEADERS})
target_link_libraries(ftlib PRIVATE ${FREETYPE_LIBRARIES} ${FTLIB_PLATFORM_LIBRARIES})
qf_set_output_dir(ftlib libs)
