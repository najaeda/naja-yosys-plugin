cmake_minimum_required(VERSION 3.21)

message(STATUS "CMake version: ${CMAKE_VERSION}")

project(yosys-naja-if)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

find_package(Yosys REQUIRED)
find_package(CapnProto REQUIRED)

add_subdirectory(src)

#enable_testing()
#add_subdirectory(tests)
