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
        if (${libs}_DEBUG)
            set(${libs}_DEBUG ${${libs}_DEBUG} debug ${lib_name})
        else()
            set(${libs}_DEBUG debug ${lib_name})
        endif()
        if (${libs}_RELEASE)
            set(${libs}_RELEASE ${${libs}_RELEASE} optimized ${release_lib_name})
        else()
            set(${libs}_RELEASE optimized ${release_lib_name})
        endif()
    endforeach()

    set(${libs} ${${libs}_RELEASE} ${${libs}_DEBUG})
endmacro()

macro(qf_add_basewf name dir)

endmacro()

macro(qf_set_output_dir_dep name dir)
    qf_set_output_dir(${name} ${dir})
    get_target_property(_dependencies ${name} LINK_LIBRARIES)
    if (_dependencies)
        foreach(_dep ${_dependencies})
            if (TARGET ${_dep})
                qf_set_output_dir(${_dep} ${dir})
            endif()
        endforeach()
    endif()
endmacro()

macro(qf_set_output_dir name dir)
    foreach (OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIGUPPERCASE)
        set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIGUPPERCASE} ${CMAKE_BINARY_DIR}/warfork-qfusion/${OUTPUTCONFIG}/${dir})
        set_target_properties(${name} PROPERTIES LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIGUPPERCASE} ${CMAKE_BINARY_DIR}/warfork-qfusion/${OUTPUTCONFIG}/${dir})
        set_target_properties(${name} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIGUPPERCASE} ${CMAKE_BINARY_DIR}/warfork-qfusion/${OUTPUTCONFIG}/${dir})
    endforeach()

    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/warfork-qfusion/${dir})
    set_target_properties(${name} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/warfork-qfusion/${dir})
    set_target_properties(${name} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/warfork-qfusion/${dir})

    # INSTALL(TARGETS ${name} 
    #     RUNTIME DESTINATION warfork-qfusion/${dir}
    #     LIBRARY DESTINATION warfork-qfusion/${dir}
    # COMPONENT shareware)
    set_property(TARGET ${name} PROPERTY IMPORTED_NO_SONAME TRUE)
endmacro()

