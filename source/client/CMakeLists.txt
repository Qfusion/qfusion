project(${QFUSION_CLIENT_NAME})

include_directories(${ZLIB_INCLUDE_DIR} ${CURL_INCLUDE_DIR})

if (USE_SDL2)
    include_directories(${SDL2_INCLUDE_DIRS})
endif()

file(GLOB CLIENT_HEADERS
    "*.h"
)

file(GLOB CLIENT_COMMON_SOURCES
    "../qcommon/*.c"
    "../server/*.c"
    "*.c"
    "../gameshared/q_*.c"
    "../qalgo/*.c"
    "../matchmaker/*.c"
)

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    include_directories("../win32/include/msvc")

    if (USE_SDL2)
        file(GLOB CLIENT_PLATFORM_SOURCES
            "../win32/win_fs.c"
            "../win32/win_net.c"
            "../win32/win_sys.c"
            "../win32/win_threads.c"
            "../null/sys_vfs_null.c"

            "../sdl/sdl_input.c"
            "../sdl/sdl_vid.c"
            "../sdl/sdl_sys.c"
            "../sdl/sdl_clipboard.c"
            "../sdl/sdl_time.c"
            "../sdl/sdl_lib.c"

            "../win32/qfusion.rc"
        )

        set(CLIENT_PLATFORM_LIBRARIES ${SDL2_LIBRARIES} "ws2_32.lib" "winmm.lib" "dxguid.lib")
    else()
        file(GLOB CLIENT_PLATFORM_SOURCES
            "../win32/win_fs.c"
            "../win32/win_lib.c"
            "../win32/win_net.c"
            "../win32/win_sys.c"
            "../win32/win_console.c"
            "../win32/win_clipboard.c"
            "../win32/win_time.c"
            "../win32/win_threads.c"
            "../null/sys_vfs_null.c"

            "../win32/win_input.c"
            "../win32/win_vid.c"
            "../win32/conproc.c"

            "../win32/qfusion.rc"
        )

        set(CLIENT_PLATFORM_LIBRARIES "ws2_32.lib" "winmm.lib" "dxguid.lib")
    endif()

    set(CLIENT_BINARY_TYPE WIN32)

    set(BUNDLE_RESOURCES "")
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(GAME_DATA_DIR Warsow_1_5_1)

    file(GLOB CLIENT_PLATFORM_SOURCES
        "../unix/unix_fs.c"
        "../unix/unix_net.c"
        "../unix/unix_sys.c"
        "../unix/unix_threads.c"
        "../null/sys_vfs_null.c"

        "../sdl/sdl_input.c"
        "../sdl/sdl_input_osx.m"
        "../sdl/sdl_vid.c"
        "../sdl/sdl_sys.c"
        "../sdl/sdl_clipboard.c"
        "../sdl/sdl_time.c"
        "../sdl/sdl_lib.c"

        "../mac/mac_sys.m"
        "../mac/Warsow-Info.plist"
    )

    set(CLIENT_PLATFORM_LIBRARIES ${SDL2_LIBRARY} "-framework IOKit" "-framework Cocoa")
    set(CLIENT_BINARY_TYPE MACOSX_BUNDLE)

    set(BUNDLE_RESOURCES
        ../mac/Frameworks
        ../../../${GAME_DATA_DIR}/basewsw
        ../../../${GAME_DATA_DIR}/docs
        ../../glsl
        ../mac/wsw_icon.icns
    )
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    if (USE_SDL2)
        file(GLOB CLIENT_PLATFORM_SOURCES
            "../unix/unix_fs.c"
            "../unix/unix_net.c"
            "../unix/unix_sys.c"
            "../unix/unix_threads.c"
            "../null/sys_vfs_null.c"

            "../sdl/sdl_input.c"
            "../sdl/sdl_vid.c"
            "../sdl/sdl_sys.c"
            "../sdl/sdl_clipboard.c"
            "../sdl/sdl_time.c"
            "../sdl/sdl_lib.c"
        )

        set(CLIENT_PLATFORM_LIBRARIES ${SDL2_LIBRARY} "pthread" "m" "dl" "rt")
    else()
        file(GLOB CLIENT_PLATFORM_SOURCES
            "../unix/unix_fs.c"
            "../unix/unix_lib.c"
            "../unix/unix_net.c"
            "../unix/unix_sys.c"
            "../unix/unix_console.c"
            "../unix/unix_clipboard.c"
            "../unix/unix_time.c"
            "../unix/unix_threads.c"
            "../null/sys_vfs_null.c"

            "../unix/keysym2ucs.c"
            "../unix/unix_input.c"
            "../unix/unix_vid.c"
        )
    
        set(CLIENT_PLATFORM_LIBRARIES "pthread" "dl" "m" "X11" "Xi" "Xext" "Xrandr" "rt")
    endif()
    set(CLIENT_BINARY_TYPE "")

    set(BUNDLE_RESOURCES "")
endif()

add_executable(${QFUSION_CLIENT_NAME} ${CLIENT_BINARY_TYPE} ${CLIENT_HEADERS} ${CLIENT_COMMON_SOURCES} ${CLIENT_PLATFORM_SOURCES} ${BUNDLE_RESOURCES})

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    add_dependencies(${QFUSION_CLIENT_NAME} angelwrap cgame cin ftlib game irc ref_gl snd_openal snd_qf ui)

    set_target_properties(${QFUSION_CLIENT_NAME} PROPERTIES MACOSX_BUNDLE_INFO_PLIST ../mac/Warsow-Info.plist)
    set_target_properties(${QFUSION_CLIENT_NAME} PROPERTIES XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@executable_path/../Frameworks @executable_path/../Resources/base @executable_path/../Resources/libs")
    set_source_files_properties(../mac/wsw_icon.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

    set_source_files_properties(../mac/Frameworks                 PROPERTIES MACOSX_PACKAGE_LOCATION .)
    set_source_files_properties(../../../${GAME_DATA_DIR}/basewsw PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_source_files_properties(../../../${GAME_DATA_DIR}/docs    PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_source_files_properties(../../glsl                        PROPERTIES MACOSX_PACKAGE_LOCATION Resources/basewsw)

    set(BUNDLE_PATH ${CMAKE_HOME_DIRECTORY}/build/$(CONFIGURATION)/${QFUSION_CLIENT_NAME}.app/Contents)
    set(COPY_DYLIBS
        cp -r ${CMAKE_HOME_DIRECTORY}/build/$(CONFIGURATION)/base/ ${BUNDLE_PATH}/Resources/basewsw/ &&
        cp -r ${CMAKE_HOME_DIRECTORY}/build/$(CONFIGURATION)/libs/ ${BUNDLE_PATH}/Resources/libs/ &&
        cd ${BUNDLE_PATH}/Resources/ &&
        ln -sfh ./basewsw/ ${BUNDLE_PATH}/Resources/base
    )
        
    add_custom_command(TARGET ${QFUSION_CLIENT_NAME} POST_BUILD COMMAND ${COPY_DYLIBS})
endif()

target_link_libraries(${QFUSION_CLIENT_NAME} cin ${CURL_LIBRARY} ${ZLIB_LIBRARY} ${CLIENT_PLATFORM_LIBRARIES})
qf_set_output_dir(${QFUSION_CLIENT_NAME} "")
