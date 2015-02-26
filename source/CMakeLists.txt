project(qfusion)

cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH ${CMAKE_HOME_DIRECTORY}/cmake)
include(QfusionUtils)

# You can override this var with commandline option -DQFUSION_GAME_DIR=name
if (NOT QFUSION_GAME_DIR)
    set(QFUSION_GAME_DIR base)
endif()

# You can override this var with commandline option -DQFUSION_CLIENT_NAME=name
if (NOT QFUSION_CLIENT_NAME)
    set(QFUSION_CLIENT_NAME qfusion)
endif()

# You can override this var with commandline option -DQFUSION_SERVER_NAME=name
if (NOT QFUSION_SERVER_NAME)
    set(QFUSION_SERVER_NAME qfusion_server)
endif()

# You can override this var with commandline option -DQFUSION_TVSERVER_NAME=name
if (NOT QFUSION_TVSERVER_NAME)
    set(QFUSION_TVSERVER_NAME qfusiontv_server)
endif()

option(USE_SDL2 "Build using SDL2" OFF)

# Windows specific settings
if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(QFUSION_WINDOWS_ARCH "x86")

    if ((MINGW AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64") OR (CMAKE_SIZEOF_VOID_P EQUAL 8))
        set(QFUSION_WINDOWS_ARCH "x64")
    endif()

    set(CMAKE_INCLUDE_PATH
        "../libsrcs/libogg"
        "../libsrcs/libvorbis"
        "../libsrcs/libtheora"
        "../libsrcs/libcurl/include"
        "../libsrcs/angelscript/sdk/angelscript"
        "../libsrcs/zlib"
        "../libsrcs/libfreetype"
        "../libsrcs/libpng"
        "../libsrcs/libjpeg"
        "../libsrcs/libRocket/libRocket/Include"
        "../libsrcs/SDL2/include"
    )

    add_definitions(-D_CRT_SECURE_NO_WARNINGS -DCURL_STATICLIB -DWIN32 -D_WINDOWS)

    if (MSVC)
        set(CMAKE_LIBRARY_PATH "win32/${QFUSION_WINDOWS_ARCH}/lib/debug")
    else()
        set(CMAKE_LIBRARY_PATH "win32/${QFUSION_WINDOWS_ARCH}/lib/mingw")
    endif()

    if (MSVC)
        add_release_link_flags("/LTCG")
        
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT /W4 /Ob1 /GL /Ot")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd /W4")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT /W4 /Ob1 /GL /Ot")
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd /W4")
    endif()
endif()

# OS X specific settings
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_OSX_ARCHITECTURES "$(ARCHS_STANDARD_64_BIT)")    # Compile only 64-bit version
    set(CMAKE_OSX_SYSROOT "macosx")                            # Compile with latest available OS X sdk
    set(CMAKE_XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET "10.8") # Use 10.8 as deployment target
    set(CMAKE_XCODE_ATTRIBUTE_GCC_C_LANGUAGE_STANDARD "c99")   # Use C99 standart for compiling C files    
    set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")      # Use modern libc++ instead of libstdc++
    set(CMAKE_FRAMEWORK_PATH "mac/Frameworks")
    set(CMAKE_MACOSX_RPATH 1)
    set(JPEG_NAMES libjpeg) # libjpeg.framework should be renamed to jpeg.framework to remove this hack
    set(USE_SDL2 1)

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-unused-function -fvisibility=hidden -Winvalid-pch")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -D_DEBUG")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fno-strict-aliasing -ffast-math -funroll-loops -msse2")

    set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++0x")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
endif()

# Linux or mingw specific settings
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR MINGW)
    # If no build type was specified - use release build settings
    if (NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release")
    endif()

    add_definitions(-D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE)

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pipe -Wall -Wno-unused-function -fvisibility=hidden -Winvalid-pch")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -D_DEBUG")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fno-strict-aliasing -ffast-math -funroll-loops -msse2 -O2")

    set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++0x")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")

    # -Wl,--as-needed
    add_common_link_flags("-Wl,--as-needed")

    # -s in release link flags
    add_release_link_flags("-s")

    if (MINGW)
        add_common_link_flags("-static -static-libgcc -static-libstdc++")
    endif()
endif()

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

find_package(Ogg REQUIRED)
find_package(Vorbis REQUIRED)
find_package(Theora REQUIRED)
find_package(CURL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Freetype REQUIRED)
find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)

# We need SDL only on non-Windows platforms
if (USE_SDL2)
    find_package(SDL2 REQUIRED)
    add_definitions(-DUSE_SDL2)
else()
    if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Windows")
        find_package(SDL REQUIRED)
    endif()
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR MINGW)
    # On Linux we build angelscript from source
    set(ANGELSCRIPT_DIR ${CMAKE_HOME_DIRECTORY}/../libsrcs/angelscript/sdk/angelscript)
    if (MINGW)
        set(ANGELSCRIPT_PROJECT_DIR ${ANGELSCRIPT_DIR}/projects/mingw)
    else()
        set(ANGELSCRIPT_PROJECT_DIR ${ANGELSCRIPT_DIR}/projects/gnuc)
    endif()
    set(ANGELSCRIPT_INCLUDE_DIR ${ANGELSCRIPT_DIR}/include/)
    set(ANGELSCRIPT_LIBRARY ${ANGELSCRIPT_DIR}/lib/libangelscript.a)

    set(ANGELSCRIPT angelscript)
    add_custom_target(angelscript CXX=${CMAKE_CXX_COMPILER} AR=${CMAKE_AR} RANLIB=${CMAKE_RANLIB} CFLAGS=${CMAKE_C_FLAGS} CXXFLAGS=${CMAKE_CXX_FLAGS} make -C ${ANGELSCRIPT_PROJECT_DIR})
else()
    find_package(Angelscript REQUIRED)
endif()

# We build libRocket from source
set(LIBROCKET_DIR ${CMAKE_HOME_DIRECTORY}/../libsrcs/libRocket/libRocket)
set(LIBROCKET_INCLUDE_DIR ${LIBROCKET_DIR}/Include)
set(LIBROCKET_SOURCE_DIR ${LIBROCKET_DIR}/Source)	

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR MINGW)
	set(LIBROCKET_LIBRARIES ${LIBROCKET_DIR}/lib/libRocketWSW.a)
	set(LIBROCKET libRocket)

	add_custom_target(libRocket CXX=${CMAKE_CXX_COMPILER} AR=${CMAKE_AR} RANLIB=${CMAKE_RANLIB} CFLAGS=${CMAKE_C_FLAGS} CXXFLAGS=${CMAKE_CXX_FLAGS} make -C ${LIBROCKET_DIR} -f Build/Makefile.qfusion)
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(CMAKE_EXECUTABLE_SUFFIX "_${QFUSION_WINDOWS_ARCH}${CMAKE_EXECUTABLE_SUFFIX}")
    set(CMAKE_SHARED_LIBRARY_PREFIX "")
    set(CMAKE_SHARED_LIBRARY_SUFFIX "_${QFUSION_WINDOWS_ARCH}${CMAKE_SHARED_LIBRARY_SUFFIX}")

    find_windows_release_libs(ZLIB_LIBRARY)
    find_windows_release_libs(JPEG_LIBRARIES)
    find_windows_release_libs(PNG_LIBRARIES)
    find_windows_release_libs(FREETYPE_LIBRARIES)
    find_windows_release_libs(OGG_LIBRARY)
    find_windows_release_libs(VORBIS_LIBRARIES)
    find_windows_release_libs(THEORA_LIBRARY)
    find_windows_release_libs(CURL_LIBRARY)
    find_windows_release_libs(ANGELSCRIPT_LIBRARY)
    find_windows_release_libs(LIBROCKET_LIBRARIES)
    find_windows_release_libs(SDL2_LIBRARIES)
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux") # Do not add 'OR MINGW' here - we need to use suffix-prefix settings from Windows
    set(CMAKE_EXECUTABLE_SUFFIX ".${CMAKE_SYSTEM_PROCESSOR}${CMAKE_EXECUTABLE_SUFFIX}")
    set(CMAKE_SHARED_LIBRARY_PREFIX "")
    set(CMAKE_SHARED_LIBRARY_SUFFIX "_${CMAKE_SYSTEM_PROCESSOR}${CMAKE_SHARED_LIBRARY_SUFFIX}")
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_SHARED_LIBRARY_SUFFIX "_mac${CMAKE_SHARED_LIBRARY_SUFFIX}")

    capitalize(QFUSION_CLIENT_NAME)
endif()

add_subdirectory(angelwrap)
add_subdirectory(cgame)
add_subdirectory(cin)
add_subdirectory(ftlib)
add_subdirectory(game)
add_subdirectory(irc)
add_subdirectory(ref_gl)
add_subdirectory(snd_openal)
add_subdirectory(snd_qf)
add_subdirectory(ui)
add_subdirectory(steamlib)
add_subdirectory(server)
add_subdirectory(tv_server)
add_subdirectory(client)
