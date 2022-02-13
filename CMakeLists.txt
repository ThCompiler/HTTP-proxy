cmake_minimum_required(VERSION 3.1X)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-Wall -Wpedantic -Werror -Wextra -ggdb3")

set(PROJECT_NAME http-proxy)
set(SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)
set(SOURCE ${SOURCE_DIR}/main.cpp lib/src/tcp_lib.cpp)
set(LIBRARIES_DIR ${CMAKE_SOURCE_DIR}/lib)
cmake_policy(SET CMP0079 NEW)

project(${PROJECT_NAME})

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake-utilits")
include(templates)

###########
# Project #
###########

add_executable(${PROJECT_NAME} ${SOURCE})

add_subdirectory("lib")

target_link_libraries(${PROJECT_NAME} base_tcp_lib pthread)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} wsock32 ws2_32)
endif()
