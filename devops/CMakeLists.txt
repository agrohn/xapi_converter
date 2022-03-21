include_directories(xapi_devops PUBLIC include ${PROJECT_SOURCE_DIR}/common/include ${CURLPP_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})
set(LIBS ${LIBS} ${CURLPP_LIBRARIES} ${CURL_LIBRARIES}  ${Boost_LIBRARIES} nlohmann_json::nlohmann_json)

set(SOURCES 
  src/xapi_commit.cpp
  src/xapi_wikiupdate.cpp
  src/xapi_workitemactivity.cpp
  src/xapi_converter_devops.cpp
  )
set(HEADERS
  include/xapi_commit.h
  include/xapi_wikiupdate.h
  include/xapi_workitemactivity.h
  include/xapi_converter_devops.h
  )

add_executable(xapi_devops ${SOURCES})

target_link_libraries(xapi_devops xapi_common ${LIBS} )
