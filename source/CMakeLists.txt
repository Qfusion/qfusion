project(qfusion)

cmake_minimum_required(VERSION 3.1)

set(CMAKE_MODULE_PATH ${CMAKE_HOME_DIRECTORY}/cmake)

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

    if (MINGW AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
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

    if (NOT MINGW)
        set(CMAKE_LIBRARY_PATH "win32/${QFUSION_WINDOWS_ARCH}/lib/debug")
    else()
        set(CMAKE_LIBRARY_PATH "win32/${QFUSION_WINDOWS_ARCH}/lib/mingw")
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
endif()

# Linux specific settings
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    # If no build type was specified - use release build settings
    if (NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release")
    endif()

    if (CMAKE_CROSSCOMPILING)
        set(CMAKE_LIBRARY_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}-linux-gnu")
        link_directories("${CMAKE_FIND_ROOT_PATH}/usr/lib/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu")
    endif()
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

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR MINGW)
    # On Linux we build libRocket from source
    set(LIBROCKET_DIR ${CMAKE_HOME_DIRECTORY}/../libsrcs/libRocket/libRocket)
    set(LIBROCKET_INCLUDE_DIR ${LIBROCKET_DIR}/Include)
    set(LIBROCKET_LIBRARIES ${LIBROCKET_DIR}/lib/libRocketWSW.a)

    set(LIBROCKET libRocket)
    add_custom_target(libRocket CXX=${CMAKE_CXX_COMPILER} AR=${CMAKE_AR} RANLIB=${CMAKE_RANLIB} CFLAGS=${CMAKE_C_FLAGS} CXXFLAGS=${CMAKE_CXX_FLAGS} make -C ${LIBROCKET_DIR} -f Build/Makefile.qfusion)
else()
    find_package(Rocket REQUIRED)
endif()

macro(find_windows_release_libs libs)
    foreach (lib_name ${${libs}})
        string(REPLACE "lib/debug" "lib/release" release_lib_name ${lib_name})
        set(${libs}_DEBUG ${${libs}_DEBUG} debug ${lib_name})
        set(${libs}_RELEASE ${${libs}_RELEASE} optimized ${release_lib_name})
    endforeach()

    set(${libs} ${${libs}_RELEASE} ${${libs}_DEBUG})
endmacro()

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(CMAKE_EXECUTABLE_SUFFIX "_${QFUSION_WINDOWS_ARCH}${CMAKE_EXECUTABLE_SUFFIX}")
    set(CMAKE_SHARED_LIBRARY_PREFIX "")
    set(CMAKE_SHARED_LIBRARY_SUFFIX "_${QFUSION_WINDOWS_ARCH}${CMAKE_SHARED_LIBRARY_SUFFIX}")

    if (MINGW)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
        set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
    else()
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
        set(CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_MODULE_LINKER_FLAGS_RELEASE} /LTCG")
        set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG")
        
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT /W4 /Ob1 /GL /Ot")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd /W4")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT /W4 /Ob1 /GL /Ot")
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd /W4")
    endif()

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

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    add_definitions(-D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE)

    set(CMAKE_EXECUTABLE_SUFFIX ".${CMAKE_SYSTEM_PROCESSOR}${CMAKE_EXECUTABLE_SUFFIX}")
    set(CMAKE_SHARED_LIBRARY_PREFIX "")
    set(CMAKE_SHARED_LIBRARY_SUFFIX "_${CMAKE_SYSTEM_PROCESSOR}${CMAKE_SHARED_LIBRARY_SUFFIX}")

    # TODO:
    # -pipe
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pipe -Wall -Wno-unused-function -fvisibility=hidden -Winvalid-pch")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -D_DEBUG")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fno-strict-aliasing -ffast-math -funroll-loops -msse2 -O2")

    set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")

    # -O1 -Wl,--as-needed
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,--as-needed")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--as-needed")
    set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} -Wl,--as-needed")

    # -s in release link flags
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -s")
    set(CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_MODULE_LINKER_FLAGS_RELEASE} -s")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -s")
    set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS_RELEASE} -s")

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

macro(capitalize s)
    string(SUBSTRING ${${s}} 0 1 s_1)
    string(SUBSTRING ${${s}} 1 -1 s_2)
    string(TOUPPER ${s_1} s_1)
    set(${s} ${s_1}${s_2})
endmacro()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-unused-function -fvisibility=hidden -Winvalid-pch")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -D_DEBUG")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fno-strict-aliasing -ffast-math -funroll-loops -msse2")

    set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")

    set(CMAKE_SHARED_LIBRARY_SUFFIX "_mac${CMAKE_SHARED_LIBRARY_SUFFIX}")

    capitalize(QFUSION_CLIENT_NAME)
endif()


macro(qf_set_output_dir name dir)
    foreach (OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIGUPPERCASE)
        set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIGUPPERCASE} ${CMAKE_HOME_DIRECTORY}/build/${OUTPUTCONFIG}/${dir})
        set_target_properties(${name} PROPERTIES LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIGUPPERCASE} ${CMAKE_HOME_DIRECTORY}/build/${OUTPUTCONFIG}/${dir})
        set_target_properties(${name} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIGUPPERCASE} ${CMAKE_HOME_DIRECTORY}/build/${OUTPUTCONFIG}/${dir})
    endforeach()

    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build/${dir})
    set_target_properties(${name} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build/${dir})
    set_target_properties(${name} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build/${dir})
endmacro()

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