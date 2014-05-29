# - Find ogg
# Find the native ogg includes and libraries
#
#  ANGELSCRIPT_INCLUDE_DIR - where to find angelscript.h, etc.
#  ANGELSCRIPT_LIBRARIES   - List of libraries when using angelscript.
#  ANGELSCRIPT_FOUND       - True if angelscript found.

if(ANGELSCRIPT_INCLUDE_DIR)
    # Already in cache, be silent
    set(ANGELSCRIPT_FIND_QUIETLY TRUE)
endif(ANGELSCRIPT_INCLUDE_DIR)
find_path(ANGELSCRIPT_INCLUDE_DIR angelscript.h PATH_SUFFIXES include)
# MSVC built angelscript may be named angelscript_static.
# The provided project files name the library with the lib prefix.
find_library(ANGELSCRIPT_LIBRARY NAMES angelscript angelscript_static libangelscript libangelscript_static)
# Handle the QUIETLY and REQUIRED arguments and set ANGELSCRIPT_FOUND
# to TRUE if all listed variables are TRUE.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ANGELSCRIPT DEFAULT_MSG ANGELSCRIPT_INCLUDE_DIR ANGELSCRIPT_LIBRARY)

set(ANGELSCRIPT_LIBRARIES ${ANGELSCRIPT_LIBRARY})

mark_as_advanced(ANGELSCRIPT_INCLUDE_DIR)
mark_as_advanced(ANGELSCRIPT_LIBRARY)