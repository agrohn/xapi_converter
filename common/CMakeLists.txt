include_directories(xapi_common PUBLIC include ${CURLPP_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})

set(SOURCES
  src/xapi_converter.cpp
  src/xapi_entry.cpp
  )
set(HEADERS
  include/xapi_entry.h
  include/xapi_errors.h
  include/xapi_statementfactory.h
  include/xapi_converter.h
  include/xapi_anonymizer.h
  )

add_library(xapi_common ${SOURCES})
add_executable( json_prettifier src/json_prettifier.cpp)
add_executable( json_parser src/json_parser.cpp)
target_link_libraries(xapi_common ${B64_LIBRARY} nlohmann_json::nlohmann_json)
