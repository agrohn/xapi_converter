# - Try to find 
# Once done this will define
#  CURLPP_FOUND - System has curlpp
#  CURLPP_INCLUDE_DIRS - The curlpp include directories
#  CURLPP_LIBRARIES - The libraries needed to use 
#  CURLPP_DEFINITIONS - Compiler switches required for using curlpp

find_package(PkgConfig)
pkg_check_modules(PC_CURLPP QUIET curlpp)
set(CURLPP_DEFINITIONS ${PC_CURLPP_CFLAGS_OTHER})

find_path(CURLPP_INCLUDE_DIR curlpp/cURLpp.hpp
          HINTS ${PC_CURLPP_INCLUDEDIR} ${PC_CURLPP_CFLAGS_OTHER}}
          PATH_SUFFIXES curlpp )

find_library(CURLPP_LIBRARY NAMES curlpp libcurlpp
             HINTS ${PC_CURLPP_LIBDIR} ${PC_CURLPP_LIBRARY_DIRS} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBXML2_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(CURLPP  DEFAULT_MSG
                                  CURLPP_LIBRARY CURLPP_INCLUDE_DIR)

mark_as_advanced(CURLPP_INCLUDE_DIR CURLPP_LIBRARY )

set(CURLPP_LIBRARIES ${CURLPP_LIBRARY} )
set(CURLPP_INCLUDE_DIRS ${CURLPP_INCLUDE_DIR} )