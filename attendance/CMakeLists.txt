include_directories(xapi_attendance PUBLIC include ${PROJECT_SOURCE_DIR}/common/include ${CURLPP_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})
set(LIBS ${LIBS} ${CURLPP_LIBRARIES} ${CURL_LIBRARIES}  ${Boost_LIBRARIES} nlohmann_json::nlohmann_json)

set(SOURCES 
  src/xapi_attend.cpp
  src/xapi_converter_attendance.cpp
  )
set(HEADERS
  include/xapi_attend.h
  include/xapi_converter_attendance.h
  )

add_executable(xapi_attendance ${SOURCES})
target_link_libraries(xapi_attendance xapi_common ${LIBS} )
