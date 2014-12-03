cmake_minimum_required(VERSION 2.6)
PROJECT(freenetconfd)
ADD_DEFINITIONS(-Os -Wall --std=gnu11 -Wmissing-declarations -D_GNU_SOURCE)
INCLUDE_DIRECTORIES(include)

OPTION(PLUGIN_SUPPORT "build plugins" ON)

FILE(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/build/modules")
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib/freenetconfd)

SET(SOURCES
	src/freenetconfd.c
	src/freenetconfd.h
	src/connection.c
	src/connection.h
	src/ubus.c
	src/ubus.h
	src/methods.c
	src/methods.h
	src/modules.c
	src/modules.h
	src/config.c
	src/config.h
	src/netconf.c
	src/netconf.h
	src/datastore.c
	src/datastore.h
)

ADD_EXECUTABLE(freenetconfd ${SOURCES})
TARGET_LINK_LIBRARIES(freenetconfd  ${CMAKE_DL_LIBS})

FIND_PACKAGE(LIBUBOX REQUIRED)
INCLUDE_DIRECTORIES(${LIBUBOX_INCLUDE_DIR})
TARGET_LINK_LIBRARIES(freenetconfd ${LIBUBOX_LIBRARIES})

FIND_PACKAGE(LIBUBUS REQUIRED)
INCLUDE_DIRECTORIES(${LIBUBUS_INCLUDE_DIR})
TARGET_LINK_LIBRARIES(freenetconfd ${LIBUBUS_LIBRARIES})

FIND_PACKAGE(LIBROXML REQUIRED)
INCLUDE_DIRECTORIES(${LIBROXML_INCLUDE_DIR})
TARGET_LINK_LIBRARIES(freenetconfd ${LIBROXML_LIBRARIES})

FIND_PACKAGE(UCI REQUIRED)
INCLUDE_DIRECTORIES(${UCI_INCLUDE_DIR})
TARGET_LINK_LIBRARIES(freenetconfd ${UCI_LIBRARIES} ${CMAKE_DL_LIBS})


SET(PLUGIN_INCLUDE_FILES src/freenetconfd.h src/datastore.h include/freenetconfd/plugin.h)

IF (PLUGIN_SUPPORT)
  ADD_LIBRARY(plugin_ietf_system MODULE plugins/ietf-system.c ${PLUGIN_INCLUDE_FILES})
  SET_TARGET_PROPERTIES(plugin_ietf_system PROPERTIES OUTPUT_NAME ietf-system PREFIX "")
  INSTALL(TARGETS plugin_ietf_system LIBRARY DESTINATION lib/freenetconfd)
ENDIF ()

INSTALL(TARGETS freenetconfd RUNTIME DESTINATION bin)
