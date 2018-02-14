cmake_minimum_required(VERSION 3.8.2)
set(CMAKE_CXX_STANDARD 14)
project(xapi_converter)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
find_package(Curlpp REQUIRED)
find_package(CURL REQUIRED)

include_directories(xapi_converter PUBLIC include ${CURLPP_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS} )
set(LIBS ${LIBS} ${CURLPP_LIBRARIES} ${CURL_LIBRARIES} )

set(SOURCES src/xapi_converter.cpp)
set(HEADERS include/json.hpp)
add_executable( xapi_converter ${HEADERS} ${SOURCES} )
# Copy data to build
file(COPY data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(xapi_converter ${LIBS})
include(CTest)
add_test(SimpleTest xapi_converter "data/simple.csv" "YOUR_LEARNINGLOCKER_SERVER")