# LIBTALLOC_FOUND - true if library and headers were found
# LIBTALLOC_INCLUDE_DIRS - include directories
# LIBTALLOC_LIBRARIES - library directories

find_package(PkgConfig)
pkg_check_modules(PC_LIBTALLOC QUIET talloc)

find_path(LIBTALLOC_INCLUDE_DIR talloc.h
	HINTS ${PC_LIBTALLOC_INCLUDEDIR} ${PC_LIBTALLOC_INCLUDE_DIRS})

find_library(LIBTALLOC_LIBRARY NAMES talloc libtalloc
	HINTS ${PC_LIBTALLOC_LIBDIR} ${PC_LIBTALLOC_LIBRARY_DIRS})

set(LIBTALLOC_LIBRARIES ${LIBTALLOC_LIBRARY})
set(LIBTALLOC_INCLUDE_DIRS ${LIBTALLOC_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(LIBTALLOC DEFAULT_MSG LIBTALLOC_LIBRARY LIBTALLOC_INCLUDE_DIR)

mark_as_advanced(LIBTALLOC_INCLUDE_DIR LIBTALLOC_LIBRARY)
