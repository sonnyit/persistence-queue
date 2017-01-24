# Specify the minimum version for CMake

cmake_minimum_required(VERSION 2.8)

# Project's name

project(example)

file(GLOB_RECURSE sources include/persist_queue/*)

# Set the output folder where your program will be created
# set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
# set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
# set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

# The following folder will be included
include_directories("include/persist_queue")

add_executable(example ${sources} example.cpp)

target_compile_options(example PUBLIC -std=c++11 -Wall)

find_package(Boost 1.36.0 COMPONENTS filesystem system REQUIRED)
find_package(Threads)

target_link_libraries(example PUBLIC
  ${Boost_LIBRARIES}
	${CMAKE_THREAD_LIBS_INIT}
  # here you can add any library dependencies
)
