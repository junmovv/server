cmake_minimum_required(VERSION 3.0)

project(miniMuduo)

set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# add_library(tinyMuduo STATIC  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

file(GLOB LOG_SRC ${CMAKE_CURRENT_SOURCE_DIR}/log/src/*.cpp)
file(GLOB MUDUO_SRC  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

set(SRC_LIST
    ${LOG_SRC}
    ${MUDUO_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

add_executable(tinyMuduo ${SRC_LIST}) 

target_link_libraries(tinyMuduo PRIVATE pthread)