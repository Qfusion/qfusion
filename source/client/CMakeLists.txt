project(${QFUSION_CLIENT_NAME})

include_directories(${FREETYPE_INCLUDE_DIR})

add_definitions(-DAL_LIBTYPE_STATIC)

file(GLOB CLIENT_HEADERS
	"*.h"
	"renderer/*.h"
	"ftlib/*.h"
	"sound/*.h"
	"../gameshared/q_*.h"
	"../gameshared/anorms.h"
	"../gameshared/config.h"
	"../qcommon/*.h"
	"../qalgo/*.h"
	"../angelwrap/qas_public.h"
	"../cgame/cg_public.h"
	"../cgame/ref.h"
	"../game/g_public.h"
	"../ui/ui_public.h"
	"../steamlib/steamlib_public.h"
	"../server/*.h"
	)

file(GLOB CLIENT_COMMON_SOURCES
	"../qcommon/*.c"
	"../server/*.c"
	"*.c"
	"../gameshared/q_*.c"
	"../qalgo/*.c"

	"renderer/*.c"
	"ftlib/*.c"
	"sound/*.c"

	"../sdl/sdl_input.c"
	"../sdl/sdl_input_joy.c"
	"../sdl/sdl_window.c"
	"../sdl/sdl_sys.c"
	"../sdl/sdl_clipboard.c"
	"../sdl/sdl_time.c"
	"../sdl/sdl_lib.c"
	"../sdl/sdl_client.c"
	)

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	file(GLOB CLIENT_PLATFORM_HEADERS
		"../win32/resource.h"
		"../win32/conproc.h"
		"../win32/winquake.h"
		"../win32/win_input.h"
		)

	file(GLOB CLIENT_PLATFORM_SOURCES
		"../win32/win_fs.c"
		"../win32/win_net.c"
		"../win32/win_sys.c"
		"../win32/win_console.c"
		"../win32/win_threads.c"
		"../null/sys_vfs_null.c"

		"../win32/qfusion.rc"
		)

	set(CLIENT_PLATFORM_LIBRARIES ${SDL2MAIN_LIBRARY} ${FREETYPE_LIBRARY} "kernel32.lib" "ws2_32.lib" "winmm.lib" "version.lib" "imm32.lib")

	set(CLIENT_BINARY_TYPE WIN32)

	set(BUNDLE_RESOURCES "")
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(CLIENT_PLATFORM_HEADERS "")

	file(GLOB CLIENT_PLATFORM_SOURCES
		"../unix/unix_fs.c"
		"../unix/unix_net.c"
		"../unix/unix_sys.c"
		"../unix/unix_console.c"
		"../unix/unix_threads.c"
		"../null/sys_vfs_null.c"
		)

	set(CLIENT_PLATFORM_LIBRARIES "pthread" "m" "dl" "rt")
	set(CLIENT_BINARY_TYPE "")

	set(BUNDLE_RESOURCES "")
endif()

add_executable(${QFUSION_CLIENT_NAME} ${CLIENT_BINARY_TYPE} ${CLIENT_HEADERS} ${CLIENT_PLATFORM_HEADERS} ${CLIENT_COMMON_SOURCES} ${CLIENT_PLATFORM_SOURCES} ${BUNDLE_RESOURCES})
add_dependencies(${QFUSION_CLIENT_NAME} angelwrap cgame game steamlib ui)

target_link_libraries(${QFUSION_CLIENT_NAME} PRIVATE ${CURL_LIBRARY} ${SDL2_LIBRARY} ${ZLIB_LIBRARY} ${FREETYPE_LIBRARY} ${OPENAL_LIBRARY} ${CLIENT_PLATFORM_LIBRARIES})
qf_set_output_dir(${QFUSION_CLIENT_NAME} "")
