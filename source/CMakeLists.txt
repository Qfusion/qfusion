project(qfusion)

cmake_minimum_required(VERSION 2.8.12...3.5)
message(STATUS "CMAKE_SYSTEM: " ${CMAKE_SYSTEM})
message(STATUS "CMAKE_SYSTEM_NAME: " ${CMAKE_SYSTEM_NAME})
message(STATUS "CMAKE_SYSTEM_VERSION: " ${CMAKE_SYSTEM_VERSION})
message(STATUS "CMAKE_SYSTEM_PROCESSOR: " ${CMAKE_SYSTEM_PROCESSOR})

set(CMAKE_MODULE_PATH ${CMAKE_HOME_DIRECTORY}/cmake)
include(QfusionUtils)
include(OutputDirectory)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_SKIP_BUILD_RPATH FALSE)
 
# when building, don't use the install RPATH already
# (but later on when installing)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH $ORIGIN/libs)
set(CMAKE_BUILD_RPATH $ORIGIN/libs:$ORIGIN)

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(QFUSION_GAME Warfork)						 
if (QFUSION_GAME)
    if (${QFUSION_GAME} MATCHES "Warfork")
        set(QFUSION_GAME_STEAMAPPID 671610)
        set(QFUSION_GAME_DIR basewf)
        set(QFUSION_CLIENT_NAME warfork)
        set(QFUSION_SERVER_NAME wf_server)
        set(QFUSION_TVSERVER_NAME wftv_server)
        set(QFUSION_APPLICATION_VERSION_HEADER \"version.h\")
        set(QFUSION_MAC_ICON ../../icons/warfork.icns)
        set(QFUSION_MAC_INFO_PLIST ../mac/Warfork-Info.plist)
    endif()
endif()

if (QFUSION_APPLICATION_VERSION_HEADER)
    add_definitions(-DAPPLICATION_VERSION_HEADER=${QFUSION_APPLICATION_VERSION_HEADER})
endif()

option(USE_SDL2 "Build using SDL2" OFF)
option(BUILD_UNIT_TEST "Build with unit test" OFF)
option(GAME_MODULES_ONLY "Only build game modules" OFF)
option(BUILD_STEAMLIB "Include the steamlib module (requires Steamworks SDK)" OFF)
option(USE_GRAPHICS_NRI "Build using Nvidia NRI" OFF)

OPTION(USE_CRASHPAD "Prefer system vorbis instead of the bundled one" OFF)
option(USE_GRAPHICS_X11 "Build using X11" ON)
option(USE_GRAPHICS_WAYLAND "Build using wayland" ON)

OPTION(USE_SYSTEM_ZLIB "Prefer system ZLIB instead of the bundled one" OFF)
OPTION(USE_SYSTEM_OPENAL "Prefer system OpenAL Soft instead of the bundled one" OFF)
OPTION(USE_SYSTEM_CURL "Prefer system cURL instead of the bundled one" OFF)
OPTION(USE_SYSTEM_SDL2 "Prefer system SDL2 instead of the bundled one" OFF)
OPTION(USE_SYSTEM_OGG "Prefer system OGG instead of the bundled one" OFF)
OPTION(USE_SYSTEM_FREETYPE "Prefer system freetype instead of the bundled one" OFF)
OPTION(USE_SYSTEM_VORBIS "Prefer system vorbis instead of the bundled one" OFF)

if (USE_CRASHPAD)
    add_definitions(-DUSE_CRASHPAD=1)
endif()

set(ANGELSCRIPT_DIR ${CMAKE_HOME_DIRECTORY}/../third-party/angelscript/sdk/angelscript)
set(ANGELSCRIPT_INCLUDE_DIR ${ANGELSCRIPT_DIR}/include/)
add_subdirectory(${ANGELSCRIPT_DIR}/projects/cmake ${ANGELSCRIPT_DIR}/lib/)

set(LIBROCKET_DIR ${CMAKE_HOME_DIRECTORY}/../third-party/libRocket)
set(LIBROCKET_INCLUDE_DIR ${LIBROCKET_DIR}/Include)
set(LIBROCKET_SOURCE_DIR ${LIBROCKET_DIR}/Source)

set(MINIZ_DIR ${CMAKE_HOME_DIRECTORY}/../third-party/miniz)
set(MINIZ_INCLUDE_DIR ${MINIZ_DIR})
set(MINIZ_SOURCE_DIR ${MINIZ_DIR})

set(UTEST_INCLUDE_DIR ${CMAKE_HOME_DIRECTORY}/../third-party/utest)

set(DISCORD_DIR ${CMAKE_HOME_DIRECTORY}/../third-party/discord)
set(DISCORD_INCLUDE_DIR ${DISCORD_DIR})

set(NRI_DIR ${CMAKE_HOME_DIRECTORY}/NRI)
set(NRI_INCLUDE_DIR ${CMAKE_HOME_DIRECTORY}/NRI/Include)

#if (NOT GAME_MODULES_ONLY AND USE_GRAPHICS_NRI)
#    set(NRI_STATIC_LIBRARY ON)
#    set(NRI_ENABLE_D3D12_SUPPORT OFF)
#    set(NRI_ENABLE_D3D11_SUPPORT OFF)
#    add_subdirectory(NRI)
#endif()

# Windows specific settings
if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(QFUSION_WINDOWS_ARCH "x86")

    if ((MINGW AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64") OR (CMAKE_SIZEOF_VOID_P EQUAL 8))
        set(QFUSION_WINDOWS_ARCH "x64")
    endif()

    add_definitions(-D_CRT_SECURE_NO_WARNINGS -DCURL_STATICLIB -DWIN32 -D_WINDOWS)

    if (MSVC)
        add_release_link_flags("/LTCG")

		set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD /W4 /Ob1 /GL /Ot")
		set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MD /W4 /Ob1 /GL /Ot")
		
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd /W4")
		set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MDd /W4")
		
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MD /W4 /Ob1 /GL /Ot")
		set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} /MD /W4 /Ob1 /GL /Ot")
		
        set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} /MD /W4")
		set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} /MD /W4")
    endif()
endif()

# OS X specific settings
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_XCODE_ATTRIBUTE_GCC_C_LANGUAGE_STANDARD "gnu99")     # Use GNU99 standart for compiling C files
    set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++11") # Use C++11 standart for compiling C++ files
    set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")          # Use modern libc++ instead of libstdc++
    set(CMAKE_MACOSX_RPATH 1)
    set(USE_SDL2 1)

    # The workaround for try_compile failing with code signing
    # since cmake-3.18.2, not required
    set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
        ${CMAKE_TRY_COMPILE_PLATFORM_VARIABLES}
        "CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED"
        "CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED")
    set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED NO)
    set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO)

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-unused-function -fvisibility=hidden -Winvalid-pch")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -D_DEBUG")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fno-strict-aliasing -ffast-math -funroll-loops -msse2")

    set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")

    if(XCODE)
        option(WF_BUILD_DSYM "Enable generation of dSYM information" OFF)
        if(WF_BUILD_DSYM)
            set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym")
            set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT[variant=Release] "dwarf-with-dsym")
            set(CMAKE_XCODE_ATTRIBUTE_GCC_GENERATE_DEBUGGING_SYMBOLS "YES")
            set(CMAKE_XCODE_ATTRIBUTE_GCC_GENERATE_DEBUGGING_SYMBOLS[variant=Release] "YES")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
            set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g")
            set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -g")
            set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}")
            set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
            set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
        else()
            set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf")
            set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT[variant=Release] "dwarf")
        endif()

        # Debug and RelWithDebInfo configurations should always generate symbols
        set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT[variant=Debug] "dwarf-with-dsym")
        set(CMAKE_XCODE_ATTRIBUTE_GENERATE_PROFILING_CODE[variant=Debug] "YES")
        set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT[variant=RelWithDebInfo] "dwarf-with-dsym")
        set(CMAKE_XCODE_ATTRIBUTE_GENERATE_PROFILING_CODE[variant=RelWithDebInfo] "YES")
    endif()
endif()

# Linux or mingw specific settings
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR MINGW)
    # If no build type was specified - use release build settings
    if (NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release")
    endif()

    option(WF_BUILD_32 "Enable 32bit builds" OFF)
    if(WF_BUILD_32)
        set(CMAKE_CROSSCOMPILING)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -m32")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -m32")
        set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    endif()

    add_definitions(-D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE)

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pipe -Wall -Wno-unused-function -fvisibility=hidden -Winvalid-pch")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -D_DEBUG")
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fno-strict-aliasing -ffast-math -funroll-loops -O2")
    else()
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fno-strict-aliasing -ffast-math -funroll-loops -msse2 -O2")
    endif()
    

    if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++11")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++11")
    endif()

    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=500 ")

    # default to sdl2 build on non-windows platforms
    set(USE_SDL2 1)
    

    # linking flags can't be used for static libs
    if (!TARGET_IS_STATIC_LIB)
        # -Wl,--as-needed
        add_common_link_flags("-Wl,--as-needed")

        # -s in release link flags
        add_release_link_flags("-s")

        if (MINGW)
            add_definitions(-D_WIN32_WINNT=_WIN32_WINNT_VISTA)
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unknown-pragmas")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-pragmas")
            add_common_link_flags("-static -static-libgcc -static-libstdc++")
        endif()

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

    ## Remove soname link flags from library linking options
    #string(REPLACE "<SONAME_FLAG><TARGET_SONAME>" "" CMAKE_C_CREATE_SHARED_LIBRARY "${CMAKE_C_CREATE_SHARED_LIBRARY}")
    #string(REPLACE "<SONAME_FLAG><TARGET_SONAME>" "" CMAKE_C_CREATE_SHARED_MODULE "${CMAKE_C_CREATE_SHARED_MODULE}")

    ## Remove -rdynamic from link options
    #string(REPLACE "-rdynamic" "" CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "${CMAKE_SHARED_LIBRARY_LINK_C_FLAGS}")
endif()

if (NOT GAME_MODULES_ONLY)
    if (USE_SDL2)
        add_definitions(-DUSE_SDL2)
    endif()
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR MINGW)
    if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
        STRING(TOLOWER "win32_${QFUSION_WINDOWS_ARCH}.tar.gz" TAR_GZ_NAME)
    else()
        STRING(TOLOWER "${CMAKE_SYSTEM_NAME}_${CMAKE_SYSTEM_PROCESSOR}.tar.gz" TAR_GZ_NAME)
    endif()
    if (NOT QFUSION_TAR_GZ_OUTPUT_DIRECTORY)
        set(TAR_GZ_DIST "${QFUSION_CLIENT_NAME}_${TAR_GZ_NAME}")
    else()
        set(TAR_GZ_DIST "${QFUSION_TAR_GZ_OUTPUT_DIRECTORY}/${QFUSION_CLIENT_NAME}_${TAR_GZ_NAME}")
    endif()
    add_custom_target(tar_gz COMMAND ${CMAKE_COMMAND} -E tar "cfvz" ${TAR_GZ_DIST} "${CMAKE_BINARY_DIR}/warfork-qfusion/")
    add_dependencies(tar_gz ${QFUSION_CLIENT_NAME} game ${QFUSION_SERVER_NAME} ${QFUSION_TVSERVER_NAME})
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(CMAKE_EXECUTABLE_SUFFIX "_${QFUSION_WINDOWS_ARCH}${CMAKE_EXECUTABLE_SUFFIX}")
    set(CMAKE_SHARED_LIBRARY_PREFIX "")
    set(CMAKE_SHARED_LIBRARY_SUFFIX "_${QFUSION_WINDOWS_ARCH}${CMAKE_SHARED_LIBRARY_SUFFIX}")
endif()

if (NOT GAME_MODULES_ONLY)
    if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
        find_windows_release_libs(FREETYPE_LIBRARIES)
        find_windows_release_libs(OGG_LIBRARY)
        find_windows_release_libs(VORBIS_LIBRARIES)
        find_windows_release_libs(OPENAL_LIBRARIES)
        find_windows_release_libs(CURL_LIBRARY)
        find_windows_release_libs(ANGELSCRIPT_LIBRARY)
        find_windows_release_libs(SDL2_LIBRARIES)
    endif()
endif()


if (${CMAKE_SYSTEM_NAME} MATCHES "Linux") # Do not add 'OR MINGW' here - we need to use suffix-prefix settings from Windows
    set(CURL_LIBRARY "")
    add_definitions(-DLIBCURL_RUNTIME)

    set(OGG_LIBRARY "")
    add_definitions(-DOGGLIB_RUNTIME)

    set(VORBIS_LIBRARIES "")
    add_definitions(-DVORBISLIB_RUNTIME)

    set(FREETYPE_LIBRARIES "")
    add_definitions(-DFREETYPELIB_RUNTIME)

    set(CMAKE_EXECUTABLE_SUFFIX ".${CMAKE_SYSTEM_PROCESSOR}${CMAKE_EXECUTABLE_SUFFIX}")
    set(CMAKE_SHARED_LIBRARY_SUFFIX "_${CMAKE_SYSTEM_PROCESSOR}${CMAKE_SHARED_LIBRARY_SUFFIX}")
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_SHARED_LIBRARY_SUFFIX "_mac${CMAKE_SHARED_LIBRARY_SUFFIX}")

    capitalize(QFUSION_CLIENT_NAME)
endif()
add_subdirectory(extern)

add_subdirectory(qcore)

add_subdirectory(ref_base)
add_subdirectory(angelwrap)
add_subdirectory(cgame)
add_subdirectory(game)
add_subdirectory(steamshim)

if (NOT GAME_MODULES_ONLY)
    add_subdirectory(cin)
    add_subdirectory(ftlib)
    if (USE_GRAPHICS_NRI)
        add_subdirectory(ref_nri)
    endif() 
    add_subdirectory(ref_gl)
    add_subdirectory(snd_openal)
    add_subdirectory(ui)
    add_subdirectory(server)
    add_subdirectory(tv_server)
    add_subdirectory(client)
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    add_custom_target(deploy
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} 
        COMMAND ${CMAKE_COMMAND} 
        -DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}
        -DBIN_DIR=${CMAKE_BINARY_DIR}/warfork-qfusion/$(Configuration)
        -DASSET_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/../assets
        -P ${CMAKE_CURRENT_SOURCE_DIR}/package_assets.cmake 
        COMMENT "packaging assets ...")
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    add_custom_target(deploy
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} 
        COMMAND ${CMAKE_COMMAND} 
        -DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}
        -DBIN_DIR=${CMAKE_BINARY_DIR}/warfork-qfusion/$(CONFIGURATION)/${QFUSION_CLIENT_NAME}.app/Contents
        -DASSET_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/../assets
        -P ${CMAKE_CURRENT_SOURCE_DIR}/package_assets.cmake 
        COMMENT "packaging assets ...")
else()
    add_custom_target(deploy
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} 
        COMMAND ${CMAKE_COMMAND} 
        -DCMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}
        -DBIN_DIR=${CMAKE_BINARY_DIR}/warfork-qfusion
        -DASSET_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/../assets
        -P ${CMAKE_CURRENT_SOURCE_DIR}/package_assets.cmake 
        COMMENT "packaging assets ...")
endif()
