include_directories(all_assignments_status PUBLIC include ${PROJECT_SOURCE_DIR}/common/include ${CURLPP_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})
set(LIBS ${LIBS} ${CURLPP_LIBRARIES} ${CURL_LIBRARIES}  ${Boost_LIBRARIES} nlohmann_json::nlohmann_json)

set(SOURCES 
  src/all_assignments_status.cpp
  )
set(HEADERS
  )

add_executable(all_assignments_status ${SOURCES})
target_link_libraries(all_assignments_status xapi_common ${LIBS} )
