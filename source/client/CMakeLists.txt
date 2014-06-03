project(qfusion)

file(GLOB CLIENT_HEADERS
    "*.h"
)

file(GLOB CLIENT_COMMON_SOURCES
    "../qcommon/*.c"
    "../server/*.c"
    "*.c"
    "../unix/unix_fs.c"
    "../unix/unix_lib.c"
    "../unix/unix_net.c"
    "../unix/unix_sys.c"
    "../unix/unix_threads.c"
    "../gameshared/q_*.c"
    "../qalgo/*.c"
    "../matchmaker/*.c"
)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    file(GLOB CLIENT_PLATFORM_SOURCES
        "../mac/SDLMain.m"
        "../mac/mac_input.m"
        "../mac/mac_vid.m"
        "../mac/mac_sys.m"
        "../mac/Warsow-Info.plist"
    )

    set(CLIENT_PLATFORM_LIBRARIES ${SDL_LIBRARY} "-framework IOKit")
    set(CLIENT_BINARY_TYPE MACOSX_BUNDLE)

    set(BUNDLE_RESOURCES
        ../mac/Frameworks
        ../../../Warsow_1_5_1/basewsw
        ../../../Warsow_1_5_1/docs
        ../../glsl
        ../mac/wsw_icon.icns
    )
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    file(GLOB CLIENT_PLATFORM_SOURCES
        "../unix/keysym2ucs.c"
        "../unix/unix_input.c"
        "../unix/unix_vid.c"
    )
    set(CLIENT_PLATFORM_LIBRARIES "pthread" "dl" "m" "X11" "Xext" "Xi" "Xrandr" "rt")
    set(CLIENT_BINARY_TYPE "")

    set(BUNDLE_RESOURCES "")
endif()

qf_add_executable(qfusion ${CLIENT_BINARY_TYPE} ${CLIENT_HEADERS} ${CLIENT_COMMON_SOURCES} ${CLIENT_PLATFORM_SOURCES} ${BUNDLE_RESOURCES})

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    add_dependencies(qfusion angelwrap cgame cin ftlib game irc ref_gl snd_openal snd_qf tv_server ui)

    set_target_properties(qfusion PROPERTIES MACOSX_BUNDLE_INFO_PLIST ../mac/Warsow-Info.plist)
    set_source_files_properties(../mac/wsw_icon.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

    set_source_files_properties(../mac/Frameworks              PROPERTIES MACOSX_PACKAGE_LOCATION .)
    set_source_files_properties(../../../Warsow_1_5/basewsw    PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_source_files_properties(../../../Warsow_1_5/docs       PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_source_files_properties(../../glsl                     PROPERTIES MACOSX_PACKAGE_LOCATION Resources/basewsw)

    set(BUNDLE_PATH ${CMAKE_HOME_DIRECTORY}/build/$(CONFIGURATION)/qfusion.app/Contents)
    set(COPY_DYLIBS
        cp -r ${CMAKE_HOME_DIRECTORY}/build/$(CONFIGURATION)/base/ ${BUNDLE_PATH}/Resources/basewsw/ &&
        cp -r ${CMAKE_HOME_DIRECTORY}/build/$(CONFIGURATION)/libs/ ${BUNDLE_PATH}/Resources/libs/ &&
        cd ${BUNDLE_PATH}/Resources/ &&
        ln -sfh ./basewsw/ ${BUNDLE_PATH}/Resources/base
    )
        
    add_custom_command(TARGET qfusion POST_BUILD COMMAND ${COPY_DYLIBS})
endif()

target_link_libraries(qfusion cin ${CURL_LIBRARY} ${ZLIB_LIBRARY} ${CLIENT_PLATFORM_LIBRARIES})
qf_set_output_dir(qfusion "")
