# - Find ogg
# Find the native ogg includes and libraries
#
#  ANGELSCRIPT_INCLUDE_DIR - where to find angelscript.h, etc.
#  ANGELSCRIPT_LIBRARIES   - List of libraries when using angelscript.
#  ANGELSCRIPT_FOUND       - True if angelscript found.

if(ROCKET_INCLUDE_DIR)
    # Already in cache, be silent
    set(ROCKET_FIND_QUIETLY TRUE)
endif(ROCKET_INCLUDE_DIR)
find_path(ROCKET_INCLUDE_DIR Rocket/Core.h)
# MSVC built angelscript may be named angelscript_static.
# The provided project files name the library with the lib prefix.
find_library(ROCKET_CORE_LIBRARY NAMES RocketCore)
find_library(ROCKET_CONTROLS_LIBRARY NAMES RocketControls)
# Handle the QUIETLY and REQUIRED arguments and set ANGELSCRIPT_FOUND
# to TRUE if all listed variables are TRUE.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ROCKET DEFAULT_MSG ROCKET_INCLUDE_DIR ROCKET_CORE_LIBRARY ROCKET_CONTROLS_LIBRARY)

set(ROCKET_LIBRARIES ${ROCKET_CORE_LIBRARY} ${ROCKET_CONTROLS_LIBRARY})

mark_as_advanced(ROCKET_INCLUDE_DIR)
mark_as_advanced(ROCKET_CORE_LIBRARY ROCKET_CONTROLS_LIBRARY)