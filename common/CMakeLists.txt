include_directories(xapi_common PUBLIC include ${CURLPP_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})

set(SOURCES
  src/xapi_converter.cpp
  src/xapi_entry.cpp
  src/base64_encode.cpp
  )
set(HEADERS
  include/json.hpp
  include/xapi_entry.h
  include/xapi_errors.h
  include/xapi_statementfactory.h
  include/xapi_converter.h
  include/xapi_anonymizer.h
  )

add_library(xapi_common ${SOURCES})
add_executable( json_prettifier src/json_prettifier.cpp)
#target_link_libraries(xapi_converter ${LIBS})