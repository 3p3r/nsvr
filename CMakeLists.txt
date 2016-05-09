CMAKE_MINIMUM_REQUIRED( VERSION 2.8 FATAL_ERROR )

PROJECT( nsvr )

SET( NSVR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "root directory" )
SET( NSVR_TESTS "${NSVR_ROOT}/tests" CACHE PATH "tests directory" )
SET( NSVR_SOURCE "${NSVR_ROOT}/source" CACHE PATH "source directory" )
SET( NSVR_INCLUDE "${NSVR_ROOT}/include" CACHE PATH "include directory" )
SET( CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${NSVR_ROOT}/tools/share/cmake" )

SET_PROPERTY( GLOBAL PROPERTY USE_FOLDERS ON)

INCLUDE_DIRECTORIES( ${NSVR_INCLUDE} )

FIND_PACKAGE( GStreamer REQUIRED )

SET( NSVR_HEADERS
  "${NSVR_INCLUDE}/nsvr.hpp"
  "${NSVR_INCLUDE}/nsvr/nsvr_base.hpp"
  "${NSVR_INCLUDE}/nsvr/nsvr_client.hpp"
  "${NSVR_INCLUDE}/nsvr/nsvr_server.hpp" )

SET( NSVR_SOURCES
  "${NSVR_SOURCE}/nsvr.cpp"
  "${NSVR_SOURCE}/nsvr/nsvr_base.cpp"
  "${NSVR_SOURCE}/nsvr/nsvr_client.cpp"
  "${NSVR_SOURCE}/nsvr/nsvr_server.cpp" )

ADD_LIBRARY( nsvr.static STATIC ${NSVR_HEADERS} ${NSVR_SOURCES} )
TARGET_ADD_GSTREAMER_MODULES( nsvr.static
	gstreamer-1.0
	gstreamer-app-1.0
	gstreamer-pbutils-1.0 )

FIND_PACKAGE( Cinder QUIET )
IF( Cinder_FOUND )

SET( TEST_TARGETS
  "basic.playback"
  "complete.playback" )

FOREACH( TEST_TARGET ${TEST_TARGETS} )
  ADD_EXECUTABLE( test.${TEST_TARGET}
    "${NSVR_TESTS}/${TEST_TARGET}.cpp" )
  TARGET_ADD_GSTREAMER_MODULES( test.${TEST_TARGET}
	gstreamer-1.0
	gstreamer-app-1.0
	gstreamer-pbutils-1.0 )
  CONFIGURE_CINDER_TARGET( test.${TEST_TARGET} )
  TARGET_LINK_LIBRARIES( test.${TEST_TARGET} nsvr.static )
ENDFOREACH()

ENDIF()