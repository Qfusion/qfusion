project(qfusion)

cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH ${CMAKE_HOME_DIRECTORY}/cmake)

set(CMAKE_FRAMEWORK_PATH "mac/Frameworks")

find_package(Ogg REQUIRED)
find_package(Vorbis REQUIRED)
find_package(Theora REQUIRED)
find_package(CURL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Freetype REQUIRED)
find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)
find_package(SDL REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-unused-function -fvisibility=hidden -Winvalid-pch")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -D_DEBUG")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fno-strict-aliasing -ffast-math -funroll-loops -msse2")

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    # On Linux we build angelscript from source
    set(ANGELSCRIPT_DIR ${CMAKE_HOME_DIRECTORY}/../libsrcs/angelscript/sdk/angelscript)
    set(ANGELSCRIPT_PROJECT_DIR ${ANGELSCRIPT_DIR}/projects/gnuc)
    set(ANGELSCRIPT_INCLUDE_DIR ${ANGELSCRIPT_DIR}/include/)
    set(ANGELSCRIPT_LIBRARY ${ANGELSCRIPT_DIR}/lib/libangelscript.a)

    set(ANGELSCRIPT angelscript)
    add_custom_target(angelscript make -C ${ANGELSCRIPT_PROJECT_DIR})
else()
    find_package(Angelscript REQUIRED)
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    # On Linux we build libRocket from source
    set(LIBROCKET_DIR ${CMAKE_HOME_DIRECTORY}/../libsrcs/libRocket/libRocket)
    set(LIBROCKET_INCLUDE_DIR ${LIBROCKET_DIR}/Include)
    set(LIBROCKET_LIBRARIES ${LIBROCKET_DIR}/lib/libRocketWSW.a)

    set(LIBROCKET libRocket)
    add_custom_target(libRocket make -C ${LIBROCKET_DIR} -f Build/Makefile.qfusion)
else()
    find_package(Rocket REQUIRED)
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

macro(qf_add_library name)
    add_library(${name} ${ARGN})

    if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        set_target_properties(${name} PROPERTIES PREFIX "")
        set_target_properties(${name} PROPERTIES SUFFIX "_x86_64${CMAKE_SHARED_LIBRARY_SUFFIX}")
    endif()

    if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        set_target_properties(${name} PROPERTIES PREFIX "")
        set_target_properties(${name} PROPERTIES SUFFIX "_mac${CMAKE_SHARED_LIBRARY_SUFFIX}")
    endif()
endmacro()

macro(qf_add_executable name)
    add_executable(${name} ${ARGN})

    if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        set_target_properties(${name} PROPERTIES PREFIX "")
        set_target_properties(${name} PROPERTIES SUFFIX ".x86_64${CMAKE_EXECUTABLE_SUFFIX}")
    endif()
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
add_subdirectory(tv_server)
add_subdirectory(ui)
add_subdirectory(client)