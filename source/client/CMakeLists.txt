project(client)

include_directories(${FREETYPE_INCLUDE_DIR})

add_definitions(-DAL_LIBTYPE_STATIC)
add_definitions(-DIMGUI_IMPL_OPENGL_LOADER_CUSTOM=\"client/renderer/glad.h\")

file(GLOB CLIENT_HEADERS
	"*.h"
	"renderer/*.h"
	"ftlib/*.h"
	"sound/*.h"
	"../gameshared/*.h"
	"../gameshared/angelwrap/*.h"
	"../gameshared/angelwrap/addon/*.h"
	"../qcommon/*.h"
	"../qalgo/*.h"
	"../cgame/*.h"
	"../game/g_public.h"
	"../server/*.h"

	"../../libs/imgui/*.h"
	)

file(GLOB CLIENT_COMMON_SOURCES
	"*.cpp"
	"../cgame/*.cpp"
	"../qcommon/*.cpp"
	"../server/sv_ccmds.cpp"
	"../server/sv_client.cpp"
	"../server/sv_demos.cpp"
	"../server/sv_game.cpp"
	"../server/sv_init.cpp"
	"../server/sv_main.cpp"
	"../server/sv_motd.cpp"
	"../server/sv_oob.cpp"
	"../server/sv_send.cpp"
	"../server/sv_web.cpp"
	"../gameshared/*.cpp"
	"../gameshared/angelwrap/*.cpp"
	"../gameshared/angelwrap/addon/*.cpp"
	"../qalgo/*.cpp"

	"ftlib/*.cpp"
	"renderer/*.cpp"
	"sdl/*.cpp"
	"sound/*.cpp"
	"../../libs/imgui/*.cpp"
	)

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	file(GLOB CLIENT_PLATFORM_HEADERS
		"../win32/resource.h"
		"../win32/winquake.h"
		"../win32/win_input.h"
		)

	file(GLOB CLIENT_PLATFORM_SOURCES
		"../win32/win_fs.cpp"
		"../win32/win_net.cpp"
		"../win32/win_client.cpp"
		"../win32/win_console.cpp"
		"../win32/win_threads.cpp"

		"../win32/client.rc"
		)

	set(CLIENT_PLATFORM_LIBRARIES ${SDL2MAIN_LIBRARY} ${FREETYPE_LIBRARY} "kernel32.lib" "ws2_32.lib" "crypt32.lib" "winmm.lib" "version.lib" "imm32.lib")

	set(CLIENT_BINARY_TYPE WIN32)
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(CLIENT_PLATFORM_HEADERS "")

	file(GLOB CLIENT_PLATFORM_SOURCES
		"../unix/unix_fs.cpp"
		"../unix/unix_net.cpp"
		"../unix/unix_console.cpp"
		"../unix/unix_threads.cpp"
		)

	set(CLIENT_PLATFORM_LIBRARIES "pthread" "m" "dl" "rt")
	set(CLIENT_BINARY_TYPE "")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Release")
	add_definitions(-DMICROPROFILE_ENABLED=0)
endif()

add_executable(client ${CLIENT_BINARY_TYPE} ${CLIENT_HEADERS} ${CLIENT_PLATFORM_HEADERS} ${CLIENT_COMMON_SOURCES} ${CLIENT_PLATFORM_SOURCES})
add_dependencies(client game)

target_link_libraries(client PRIVATE ${CURL_LIBRARIES} ${SDL2_LIBRARY} ${ZLIB_LIBRARY} ${FREETYPE_LIBRARY} ${OPENAL_LIBRARY} ${ANGELSCRIPT_LIBRARY} ${CLIENT_PLATFORM_LIBRARIES})
qf_set_output_dir(client "")
