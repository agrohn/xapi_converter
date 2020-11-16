# - Try to find 
# Once done this will define
#  B64_FOUND - System has curlpp
#  B64_INCLUDE_DIRS - The curlpp include directories
#  B64_LIBRARIES - The libraries needed to use 
#  B64_DEFINITIONS - Compiler switches required for using curlpp

find_package(PkgConfig)
pkg_check_modules(PC_B64 QUIET curlpp)
set(B64_DEFINITIONS ${PC_B64_CFLAGS_OTHER})

find_path(B64_INCLUDE_DIR b64/encode.h
          HINTS ${PC_B64_INCLUDEDIR} ${PC_B64_CFLAGS_OTHER}}
          PATH_SUFFIXES b64 )

find_library(B64_LIBRARY NAMES b64 libb64
             HINTS ${PC_B64_LIBDIR} ${PC_B64_LIBRARY_DIRS} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBXML2_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(B64  DEFAULT_MSG
                                  B64_LIBRARY B64_INCLUDE_DIR)

mark_as_advanced(B64_INCLUDE_DIR B64_LIBRARY )

set(B64_LIBRARIES ${B64_LIBRARY} )
set(B64_INCLUDE_DIRS ${B64_INCLUDE_DIR} )