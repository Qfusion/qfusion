# - Find Rocket
# Find the native libRocket includes and libraries
#
#  LIBROCKET_INCLUDE_DIR - where to find Rocket/Core.h, etc.
#  LIBROCKET_LIBRARIES   - List of libraries when using libRocket.
#  LIBROCKET_FOUND       - True if libRocket found.

if(LIBROCKET_INCLUDE_DIR)
    # Already in cache, be silent
    set(LIBROCKET_FIND_QUIETLY TRUE)
endif(LIBROCKET_INCLUDE_DIR)
find_path(LIBROCKET_INCLUDE_DIR Rocket/Core.h)
# The provided project files name the library with the lib prefix.
find_library(LIBROCKET_CORE_LIBRARY NAMES RocketCore Rocket)
if (${LIBROCKET_CORE_LIBRARY} MATCHES ".framework")
    set(LIBROCKET_CONTROLS_LIBRARY ${LIBROCKET_CORE_LIBRARY})
else()
    find_library(LIBROCKET_CONTROLS_LIBRARY NAMES RocketControls)
endif()
# Handle the QUIETLY and REQUIRED arguments and set ROCKET_FOUND
# to TRUE if all listed variables are TRUE.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ROCKET DEFAULT_MSG LIBROCKET_INCLUDE_DIR LIBROCKET_CORE_LIBRARY LIBROCKET_CONTROLS_LIBRARY)

set(LIBROCKET_LIBRARIES ${LIBROCKET_CORE_LIBRARY} ${LIBROCKET_CONTROLS_LIBRARY})

mark_as_advanced(LIBROCKET_INCLUDE_DIR)
mark_as_advanced(LIBROCKET_CORE_LIBRARY LIBROCKET_CONTROLS_LIBRARY)