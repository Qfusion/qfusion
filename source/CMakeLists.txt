project(qfusion)

cmake_minimum_required(VERSION 2.8.12)

message(STATUS "CMAKE_SYSTEM: " ${CMAKE_SYSTEM})
message(STATUS "CMAKE_SYSTEM_NAME: " ${CMAKE_SYSTEM_NAME})
message(STATUS "CMAKE_SYSTEM_VERSION: " ${CMAKE_SYSTEM_VERSION})
message(STATUS "CMAKE_SYSTEM_PROCESSOR: " ${CMAKE_SYSTEM_PROCESSOR})

set(CMAKE_MODULE_PATH ${CMAKE_HOME_DIRECTORY}/cmake)
include(QfusionUtils)

set(QFUSION_GAME_DIR base)
set(QFUSION_CLIENT_NAME client)
set(QFUSION_SERVER_NAME server)

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Debug")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Release")
	add_definitions(-DPUBLIC_BUILD)
endif()

option(GAME_MODULES_ONLY "Only build game modules" OFF)
option(SERVER_ONLY "Only build server binaries and game modules" OFF)

# We third-party libs from source

set(ANGELSCRIPT_DIR ${CMAKE_HOME_DIRECTORY}/../third-party/angelscript/sdk/angelscript)
set(ANGELSCRIPT_INCLUDE_DIR ${ANGELSCRIPT_DIR}/include/)
add_subdirectory(${ANGELSCRIPT_DIR}/projects/cmake ${ANGELSCRIPT_DIR}/lib/)

set(LIBROCKET_DIR ${CMAKE_HOME_DIRECTORY}/../third-party/libRocket)
set(LIBROCKET_INCLUDE_DIR ${LIBROCKET_DIR}/Include)
set(LIBROCKET_SOURCE_DIR ${LIBROCKET_DIR}/Source)

set(NANOSVG_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/../libs-party/nanosvg)

add_definitions(-DCURL_STATICLIB)
include_directories("${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/../libs")

# Windows specific settings
if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	add_definitions(-D_CRT_SECURE_NO_WARNINGS -DWIN32 -D_WINDOWS)

	set(CMAKE_INCLUDE_PATH
		"../libsrcs/src/libogg"
		"../libsrcs/src/libvorbis"
		"../libsrcs/src/zlib"
		)

	if (CMAKE_BUILD_TYPE STREQUAL "Release")
		set(LIBS_ARCH "windows-release")
		set(CMAKE_LIBRARY_PATH "../libsrcs/lib/win32/x64/release")
	else()
		set(LIBS_ARCH "windows-debug")
		set(CMAKE_LIBRARY_PATH "../libsrcs/lib/win32/x64/debug")
	endif()

	if (MSVC)
		add_release_link_flags("/LTCG")

		set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD /W4 /Ob1 /GL /Ot")
		set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd /W4")
		set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MD /W4 /Ob1 /GL /Ot")
		set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MDd /W4")
		set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MD /W4")
		set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} /MD /W4")
		set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} /MD /W4")
		set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} /MD /W4")
	endif()
endif()

# Linux or mingw specific settings
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	add_definitions(-D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE)

	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pipe -Wall -Wno-unused-function -Werror=implicit-function-declaration -fvisibility=hidden -fno-strict-aliasing -Winvalid-pch")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -D_DEBUG")
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -ffast-math -msse2 -O2")

	set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++11")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")

	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=500")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -no-pie")

	if (CMAKE_BUILD_TYPE STREQUAL "Release")
		set(LIBS_ARCH "linux-release")
	else()
		set(LIBS_ARCH "linux-debug")
	endif()

	# -Wl,--as-needed
	add_common_link_flags("-Wl,--as-needed")

	# -s in release link flags
	add_release_link_flags("-s")
endif()

set(CURL_LIBRARY "${CMAKE_SOURCE_DIR}/../libs/curl/${LIBS_ARCH}/${CMAKE_STATIC_LIBRARY_PREFIX}curl${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(FREETYPE_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../libs/freetype")
set(FREETYPE_LIBRARY "${CMAKE_SOURCE_DIR}/../libs/freetype/${LIBS_ARCH}/${CMAKE_STATIC_LIBRARY_PREFIX}freetype${CMAKE_STATIC_LIBRARY_SUFFIX}")
# set(LIBROCKET_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../libs/librocket")
# set(LIBROCKET_LIBRARY "${CMAKE_SOURCE_DIR}/../libs/librocket/${LIBS_ARCH}/${CMAKE_STATIC_LIBRARY_PREFIX}librocket${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(OPENAL_LIBRARY "${CMAKE_SOURCE_DIR}/../libs/openal/${LIBS_ARCH}/${CMAKE_STATIC_LIBRARY_PREFIX}openal${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(SDL2_LIBRARY "${CMAKE_SOURCE_DIR}/../libs/sdl/${LIBS_ARCH}/${CMAKE_STATIC_LIBRARY_PREFIX}SDL2${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(SDL2MAIN_LIBRARY "${CMAKE_SOURCE_DIR}/../libs/sdl/${LIBS_ARCH}/${CMAKE_STATIC_LIBRARY_PREFIX}SDL2main${CMAKE_STATIC_LIBRARY_SUFFIX}")

# Linux-only specific settings
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	if (CMAKE_CROSSCOMPILING)
		set(CMAKE_LIBRARY_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}-linux-gnu")
		link_directories("${CMAKE_FIND_ROOT_PATH}/usr/lib/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu")
	endif()

	# Remove compilating flags from link options
	# string(REPLACE "<FLAGS>" "" CMAKE_C_LINK_EXECUTABLE "${CMAKE_C_LINK_EXECUTABLE}")
	# string(REPLACE "<LANGUAGE_COMPILE_FLAGS>" "" CMAKE_C_CREATE_SHARED_LIBRARY "${CMAKE_C_CREATE_SHARED_LIBRARY}")
	# string(REPLACE "<LANGUAGE_COMPILE_FLAGS>" "" CMAKE_C_CREATE_SHARED_MODULE "${CMAKE_C_CREATE_SHARED_MODULE}")

	# Remove soname link flags from library linking options
	string(REPLACE "<SONAME_FLAG><TARGET_SONAME>" "" CMAKE_C_CREATE_SHARED_LIBRARY "${CMAKE_C_CREATE_SHARED_LIBRARY}")
	string(REPLACE "<SONAME_FLAG><TARGET_SONAME>" "" CMAKE_C_CREATE_SHARED_MODULE "${CMAKE_C_CREATE_SHARED_MODULE}")

	# Remove -rdynamic from link options
	string(REPLACE "-rdynamic" "" CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "${CMAKE_SHARED_LIBRARY_LINK_C_FLAGS}")
endif()

if (NOT GAME_MODULES_ONLY)
	find_package(ZLIB REQUIRED)

	if (NOT SERVER_ONLY)
		find_package(Ogg REQUIRED)
		find_package(Vorbis REQUIRED)
	endif()
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
		STRING(TOLOWER "win32_x64.tar.gz" TAR_GZ_NAME)
	else()
		STRING(TOLOWER "${CMAKE_SYSTEM_NAME}_${CMAKE_SYSTEM_PROCESSOR}.tar.gz" TAR_GZ_NAME)
	endif()
	if (NOT QFUSION_TAR_GZ_OUTPUT_DIRECTORY)
		set(TAR_GZ_DIST "${QFUSION_CLIENT_NAME}_${TAR_GZ_NAME}")
	else()
		set(TAR_GZ_DIST "${QFUSION_TAR_GZ_OUTPUT_DIRECTORY}/${QFUSION_CLIENT_NAME}_${TAR_GZ_NAME}")
	endif()
	add_custom_target(tar_gz COMMAND ${CMAKE_COMMAND} -E tar "cfvz" ${TAR_GZ_DIST} "${CMAKE_HOME_DIRECTORY}/build/")
	add_dependencies(tar_gz ${QFUSION_CLIENT_NAME} game ${QFUSION_SERVER_NAME})
endif()

if (NOT GAME_MODULES_ONLY)
	if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
		find_windows_release_libs(ZLIB_LIBRARY)
		find_windows_release_libs(ANGELSCRIPT_LIBRARY)
		if (NOT SERVER_ONLY)
			find_windows_release_libs(OGG_LIBRARY)
			find_windows_release_libs(VORBIS_LIBRARIES)
		endif()
	endif()
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux") # Do not add 'OR MINGW' here - we need to use suffix-prefix settings from Windows
	set(OGG_LIBRARY "")
	add_definitions(-DOGGLIB_RUNTIME)

	set(VORBIS_LIBRARIES "")
	add_definitions(-DVORBISLIB_RUNTIME)

	set(ZLIB_LIBRARY "")
	add_definitions(-DZLIB_RUNTIME)
endif()

add_subdirectory(angelwrap)
add_subdirectory(cgame)
add_subdirectory(game)

if (NOT GAME_MODULES_ONLY)
	add_subdirectory(server)

	if (NOT SERVER_ONLY)
		add_subdirectory(ui)
		add_subdirectory(steamlib)
		add_subdirectory(client)
	endif()
endif()
