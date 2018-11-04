project(${QFUSION_CLIENT_NAME})

include_directories(${ZLIB_INCLUDE_DIR} ${CURL_INCLUDE_DIR} ${SDL2_INCLUDE_DIR} ${CURL_INCLUDE_DIR} ${ZLIB_INCLUDE_DIR} ${FREETYPE_INCLUDE_DIRS})

if (NOT QFUSION_MAC_ICON)
	set(QFUSION_MAC_ICON ../../icons/qfusion.icns)
endif()
if (NOT QFUSION_MAC_INFO_PLIST)
	set(QFUSION_MAC_INFO_PLIST ../mac/Qfusion-Info.plist)
endif()

file(GLOB CLIENT_HEADERS
	"*.h"
	"renderer/*.h"
	"ftlib/*.h"
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
	"../matchmaker/*.h"
	"../server/*.h"
	)

file(GLOB CLIENT_COMMON_SOURCES
	"../qcommon/*.c"
	"../server/*.c"
	"*.c"
	"../gameshared/q_*.c"
	"../qalgo/*.c"
	"../matchmaker/*.c"

	"renderer/*.c"
	"ftlib/*.c"

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

	set(CLIENT_PLATFORM_LIBRARIES ${SDL2_LIBRARY} ${SDL2MAIN_LIBRARY} "ws2_32.lib" "winmm.lib")

	set(CLIENT_BINARY_TYPE WIN32)

	set(BUNDLE_RESOURCES "")
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set(CLIENT_PLATFORM_HEADERS "")

	file(GLOB CLIENT_PLATFORM_SOURCES
		"../unix/unix_fs.c"
		"../unix/unix_net.c"
		"../unix/unix_sys.c"
		"../unix/unix_console.c"
		"../unix/unix_threads.c"
		"../null/sys_vfs_null.c"

		"../sdl/sdl_input_osx.m"

		"../mac/mac_sys.m"
		"${QFUSION_MAC_INFO_PLIST}"
		)

	set(CLIENT_PLATFORM_LIBRARIES ${SDL2_LIBRARY} "-framework IOKit" "-framework Cocoa")
	set(CLIENT_BINARY_TYPE MACOSX_BUNDLE)

	set(BUNDLE_RESOURCES
		../../libsrcs/lib/mac/Frameworks
		${QFUSION_MAC_ICON}
		)
	if (QFUSION_DATA_DIR)
		list(APPEND BUNDLE_RESOURCES
			${QFUSION_DATA_DIR}/${QFUSION_GAME_DIR}
			${QFUSION_DATA_DIR}/docs
			)
	endif()
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

	set(CLIENT_PLATFORM_LIBRARIES ${SDL2_LIBRARY} "pthread" "m" "dl" "rt")
	set(CLIENT_BINARY_TYPE "")

	set(BUNDLE_RESOURCES "")
endif()

add_executable(${QFUSION_CLIENT_NAME} ${CLIENT_BINARY_TYPE} ${CLIENT_HEADERS} ${CLIENT_PLATFORM_HEADERS} ${CLIENT_COMMON_SOURCES} ${CLIENT_PLATFORM_SOURCES} ${BUNDLE_RESOURCES})
add_dependencies(${QFUSION_CLIENT_NAME} angelwrap cgame game snd_openal snd_qf steamlib ui)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set_target_properties(${QFUSION_CLIENT_NAME} PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${QFUSION_MAC_INFO_PLIST})
	set_target_properties(${QFUSION_CLIENT_NAME} PROPERTIES XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@executable_path/../Frameworks @executable_path/../Resources/base @executable_path/../Resources/libs")
	set_source_files_properties(${QFUSION_MAC_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

	set_source_files_properties(../mac/Frameworks                 PROPERTIES MACOSX_PACKAGE_LOCATION .)
	if (QFUSION_DATA_DIR)
		set_source_files_properties(${QFUSION_DATA_DIR}/${QFUSION_GAME_DIR}    PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
		set_source_files_properties(${QFUSION_DATA_DIR}/docs    PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
	endif()

	set(BUNDLE_PATH ${CMAKE_HOME_DIRECTORY}/build/$(CONFIGURATION)/${QFUSION_CLIENT_NAME}.app/Contents)
	set(COPY_DYLIBS
		cp -r ${CMAKE_HOME_DIRECTORY}/build/$(CONFIGURATION)/${QFUSION_GAME_DIR}/ ${BUNDLE_PATH}/Resources/${QFUSION_GAME_DIR}/ &&
		cp -r ${CMAKE_HOME_DIRECTORY}/build/$(CONFIGURATION)/libs/ ${BUNDLE_PATH}/Resources/libs/
		)

	add_custom_command(TARGET ${QFUSION_CLIENT_NAME} POST_BUILD COMMAND ${COPY_DYLIBS})
endif()

target_link_libraries(${QFUSION_CLIENT_NAME} PRIVATE ${CURL_LIBRARY} ${ZLIB_LIBRARY} ${CLIENT_PLATFORM_LIBRARIES})
qf_set_output_dir(${QFUSION_CLIENT_NAME} "")
