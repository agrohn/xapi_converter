include_directories(xapi_vipuvarsi PUBLIC include ${PROJECT_SOURCE_DIR}/common/include ${CURLPP_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})
set(LIBS ${LIBS} ${CURLPP_LIBRARIES} ${CURL_LIBRARIES} ${Boost_LIBRARIES})

set(SOURCES 
  src/xapi_converter_vipuvarsi.cpp
  src/xapi_credits.cpp
  )
set(HEADERS
  include/xapi_converter_vipuvarsi.h
  include/xapi_credits.h
  )


add_executable(xapi_vipuvarsi ${SOURCES})
target_link_libraries(xapi_vipuvarsi xapi_common ${LIBS} )