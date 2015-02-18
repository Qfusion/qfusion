# Different helper functions for cmake scripts

macro(add_common_link_flags flags)
    set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} ${flags}")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${flags}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${flags}")
    set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} ${flags}")
endmacro()

macro(add_release_link_flags flags)
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG    "${CMAKE_EXE_LINKER_FLAGS_DEBUG} ${flags}")
    set(CMAKE_MODULE_LINKER_FLAGS_DEBUG "${CMAKE_MODULE_LINKER_FLAGS_DEBUG} ${flags}")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} ${flags}")
    set(CMAKE_STATIC_LINKER_FLAGS_DEBUG "${CMAKE_STATIC_LINKER_FLAGS_DEBUG} ${flags}")
endmacro()

macro(add_release_link_flags flags)
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE    "${CMAKE_EXE_LINKER_FLAGS_RELEASE} ${flags}")
    set(CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_MODULE_LINKER_FLAGS_RELEASE} ${flags}")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} ${flags}")
    set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS_RELEASE} ${flags}")
endmacro()

macro(capitalize s)
    string(SUBSTRING ${${s}} 0 1 s_1)
    string(SUBSTRING ${${s}} 1 -1 s_2)
    string(TOUPPER ${s_1} s_1)
    set(${s} ${s_1}${s_2})
endmacro()

macro(find_windows_release_libs libs)
    foreach (lib_name ${${libs}})
        string(REPLACE "lib/debug" "lib/release" release_lib_name ${lib_name})
        set(${libs}_DEBUG ${${libs}_DEBUG} debug ${lib_name})
        set(${libs}_RELEASE ${${libs}_RELEASE} optimized ${release_lib_name})
    endforeach()

    set(${libs} ${${libs}_RELEASE} ${${libs}_DEBUG})
endmacro()