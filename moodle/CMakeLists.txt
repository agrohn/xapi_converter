include_directories(xapi_moodler PUBLIC include ${PROJECT_SOURCE_DIR}/common/include ${CURLPP_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})
set(LIBS ${LIBS} ${CURLPP_LIBRARIES} ${CURL_LIBRARIES}  ${Boost_LIBRARIES})

set(SOURCES 
  src/xapi_converter_moodle.cpp
  src/xapi_grade.cpp
  src/xapi_activityentry.cpp
  src/xapi_statementfactory.cpp
  src/xapi_assignmentinit.cpp
  )
set(HEADERS
  include/xapi_activityentry.h
  include/xapi_assignmentinit.h
  include/xapi_grade.h
  include/xapi_converter_moodle.h
  )

add_executable(xapi_moodler ${SOURCES})
add_executable(xapi_usercache_combiner src/xapi_usercachecombiner.cpp src/xapi_usercacheapp.cpp)
add_executable(xapi_usercache_diff src/xapi_usercache_diff.cpp src/xapi_usercacheapp.cpp)
target_link_libraries(xapi_moodler xapi_common ${LIBS} )
target_link_libraries(xapi_usercache_combiner ${Boost_LIBRARIES} )
target_link_libraries(xapi_usercache_diff ${Boost_LIBRARIES} )
