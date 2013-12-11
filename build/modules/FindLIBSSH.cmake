# LIBSSH_FOUND - true if library and headers were found
# LIBSSH_INCLUDE_DIRS - include directories
# LIBSSH_LIBRARIES - library directories

find_package(PkgConfig)
pkg_check_modules(PC_LIBSSH QUIET libssh)

find_path(LIBSSH_INCLUDE_DIR libssh.h
	HINTS ${PC_LIBSSH_INCLUDEDIR} ${PC_LIBSSH_INCLUDE_DIRS} PATH_SUFFIXES libssh)

find_library(LIBSSH_LIBRARY NAMES ssh libssh
	HINTS ${PC_LIBSSH_LIBDIR} ${PC_LIBSSH_LIBRARY_DIRS})

set(LIBSSH_LIBRARIES ${LIBSSH_LIBRARY})
set(LIBSSH_INCLUDE_DIRS ${LIBSSH_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(LIBSSH DEFAULT_MSG LIBSSH_LIBRARY LIBSSH_INCLUDE_DIR)

mark_as_advanced(LIBSSH_INCLUDE_DIR LIBSSH_LIBRARY)
