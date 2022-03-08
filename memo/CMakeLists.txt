include_directories(xapi_memo PUBLIC include ${PROJECT_SOURCE_DIR}/common/include ${CURLPP_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})
set(LIBS ${LIBS} ${CURLPP_LIBRARIES} ${CURL_LIBRARIES}  ${Boost_LIBRARIES} nlohmann_json::nlohmann_json)

set(SOURCES 
  src/xapi_memo.cpp
  src/xapi_converter_memo.cpp
  )
set(HEADERS
  include/xapi_memo.h
  include/xapi_converter_memo.h
  )

add_executable(xapi_memo ${SOURCES})
target_link_libraries(xapi_memo xapi_common ${LIBS} )
