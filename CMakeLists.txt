cmake_minimum_required (VERSION 2.6)
project (iocoro)

enable_testing()

set (CMAKE_CXX_STANDARD 14)
set(Boost_USE_STATIC_LIBS        ON) # only find static libs
set(Boost_USE_MULTITHREADED      ON)
set(Boost_USE_STATIC_RUNTIME    OFF)

find_package(Boost 1.65 COMPONENTS context thread REQUIRED)
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
include_directories(src)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")

add_subdirectory(src)
add_subdirectory(test)

