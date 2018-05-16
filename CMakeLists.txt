cmake_minimum_required(VERSION 3.8.2)
set(CMAKE_CXX_STANDARD 14)
project(xapi_converter)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
find_package(Curlpp REQUIRED)
find_package(CURL REQUIRED)
find_package(Boost REQUIRED program_options )

include_directories(xapi_converter PUBLIC include ${CURLPP_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})
set(LIBS ${LIBS} ${CURLPP_LIBRARIES} ${CURL_LIBRARIES} ${Boost_LIBRARIES})

set(SOURCES
  src/xapi_converter.cpp
  src/xapi_grade.cpp
  src/xapi_entry.cpp
  src/xapi_activityentry.cpp
  src/xapi_statementfactory.cpp
  src/base64_encode.cpp
  )
set(HEADERS
  include/json.hpp
  include/xapi_entry.h
  include/xapi_activityentry.h
  include/xapi_grade.h
  include/xapi_errors.h
  include/xapi_statementfactory.h
  include/xapi_converter.h
  )
add_executable( xapi_converter ${HEADERS} ${SOURCES} )
# Copy data to build
file(COPY data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(xapi_converter ${LIBS})
include(CTest)
add_test(SimpleTest xapi_converter "data/simple.csv" "YOUR_LEARNINGLOCKER_SERVER")