# LIBUBUS_FOUND - true if library and headers were found
# LIBUBUS_INCLUDE_DIRS - include directories
# LIBUBUS_LIBRARIES - library directories

find_package(PkgConfig)
pkg_check_modules(PC_LIBUBUS QUIET ubus)

find_path(LIBUBUS_INCLUDE_DIR libubus.h
	HINTS ${PC_LIBUBUS_INCLUDEDIR} ${PC_LIBUBUS_INCLUDE_DIRS})

find_library(LIBUBUS_LIBRARY NAMES ubus libubus
	HINTS ${PC_LIBUBUS_LIBDIR} ${PC_LIBUBUS_LIBRARY_DIRS})

set(LIBUBUS_LIBRARIES ${LIBUBUS_LIBRARY})
set(LIBUBUS_INCLUDE_DIRS ${LIBUBUS_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(LIBUBUS DEFAULT_MSG LIBUBUS_LIBRARY LIBUBUS_INCLUDE_DIR)

mark_as_advanced(LIBUBUS_INCLUDE_DIR LIBUBUS_LIBRARY)
