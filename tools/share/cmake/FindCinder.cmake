INCLUDE(FindPackageHandleStandardArgs)

# Finding Cinder root directory
FIND_PATH( Cinder_ROOT_DIR
  HINTS ENV CINDER_090
  NAMES "include/cinder/Cinder.h"
  DOC "Path to the root of Cinder"
  NO_DEFAULT_PATH )

IF( Cinder_ROOT_DIR )

  FIND_PATH( Cinder_INCLUDE_DIR
    PATHS ${Cinder_ROOT_DIR}
    NAMES "cinder/Cinder.h"
    PATH_SUFFIXES "include"
    DOC "Path to Cinder include directory."
    NO_DEFAULT_PATH )

  FIND_PATH( Cinder_LIBRARY_DIR
    PATHS ${Cinder_ROOT_DIR}
    NAMES "msw"
    PATH_SUFFIXES "lib"
    DOC "Path to Cinder library directory."
    NO_DEFAULT_PATH )

ENDIF()

# Find Cinder's Boost
IF( MSVC )

# CPU Architecture detection
INCLUDE( DetectTargetArch REQUIRED )
DETECT_TARGET_ARCH( ARCH )

IF ( ARCH STREQUAL "i386" )
    SET ( Cinder_BOOST_ARCH "x86" )
ELSEIF( ARCH STREQUAL "x86_64" )
    SET ( Cinder_BOOST_ARCH "x64" )
ENDIF()

SET( BOOST_LIBRARYDIR
  "${Cinder_LIBRARY_DIR}/msw/${Cinder_BOOST_ARCH}")
SET( BOOST_COMPONENTS filesystem system )
ENDIF()

SET( BOOST_INCLUDEDIR "${Cinder_INCLUDE_DIR}" )
SET( Boost_USE_STATIC_RUNTIME ON )
SET( Boost_USE_MULTITHREADED ON )
SET( Boost_USE_STATIC_LIBS ON )

FIND_PACKAGE( Boost QUIET COMPONENTS ${BOOST_COMPONENTS} )

IF ( NOT Boost_FOUND )
MESSAGE( WARNING
  "Unable to find Cinder's bundled Boost. "
  "Are you trying to build with Cinder on a non-2013 Visual Studio?" )
ENDIF()

FIND_PACKAGE_HANDLE_STANDARD_ARGS(
  Cinder DEFAULT_MSG
  Cinder_INCLUDE_DIR
  Cinder_LIBRARY_DIR
  Boost_FOUND )

IF( Cinder_FOUND )
# Until Cinder starts to officially support CMake, this is how it's done
INCLUDE_EXTERNAL_MSPROJECT(cinder
  "${Cinder_ROOT_DIR}/vc2013/cinder.vcxproj")

INCLUDE_DIRECTORIES( "${Cinder_INCLUDE_DIR}" ) # Cinder 0.9.0
# Link Boost directory
LINK_DIRECTORIES( "${BOOST_LIBRARYDIR}" )

# Make sure everything is under /MT[d], /MD[d] is not cinder compatible
SET( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT" )
SET( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd" )

MACRO( CONFIGURE_CINDER_TARGET target_name )

ADD_DEPENDENCIES( ${target_name} cinder )
SET_TARGET_PROPERTIES( ${target_name} PROPERTIES
  LINK_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS"
  LINK_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /NODEFAULTLIB:\"LIBCMT\""
  COMPILE_DEFINITIONS "${COMPILE_DEFINITIONS} -DUNICODE -D_UNICODE"
  CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd"
  CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT" )

ENDMACRO()

# Unicode character set and disable annoying POSIX deprecated warnings
ADD_DEFINITIONS( -DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS )
ENDIF()
