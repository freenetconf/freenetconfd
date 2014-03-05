# LIBEV_FOUND - true if library and headers were found
# LIBEV_INCLUDE_DIRS - include directories
# LIBEV_LIBRARIES - library directories

find_package(PkgConfig)
pkg_check_modules(PC_LIBEV QUIET libev)

find_path(LIBEV_INCLUDE_DIR ev.h
	HINTS ${PC_LIBEV_INCLUDEDIR} ${PC_LIBEV_INCLUDE_DIRS} PATH_SUFFIXES ev)

find_library(LIBEV_LIBRARY NAMES ev libev
	HINTS ${PC_LIBEV_LIBDIR} ${PC_LIBEV_LIBRARY_DIRS})

set(LIBEV_LIBRARIES ${LIBEV_LIBRARY})
set(LIBEV_INCLUDE_DIRS ${LIBEV_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(LIBEV DEFAULT_MSG LIBEV_LIBRARY LIBEV_INCLUDE_DIR)

mark_as_advanced(LIBEV_INCLUDE_DIR LIBEV_LIBRARY)
