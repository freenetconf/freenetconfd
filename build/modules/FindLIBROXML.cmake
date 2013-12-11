# LIBROXML_FOUND - true if library and headers were found
# LIBROXML_INCLUDE_DIRS - include directories
# LIBROXML_LIBRARIES - library directories

find_package(PkgConfig)
pkg_check_modules(PC_LIBROXML QUIET libroxml)

find_path(LIBROXML_INCLUDE_DIR roxml.h
	HINTS ${PC_LIBROXML_INCLUDEDIR} ${PC_LIBROXML_INCLUDE_DIRS})

find_library(LIBROXML_LIBRARY NAMES roxml libroxml
	HINTS ${PC_LIBROXML_LIBDIR} ${PC_LIBROXML_LIBRARY_DIRS})

set(LIBROXML_LIBRARIES ${LIBROXML_LIBRARY})
set(LIBROXML_INCLUDE_DIRS ${LIBROXML_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(LIBROXML DEFAULT_MSG LIBROXML_LIBRARY LIBROXML_INCLUDE_DIR)

mark_as_advanced(LIBROXML_INCLUDE_DIR LIBROXML_LIBRARY)
