cmake_minimum_required (VERSION 2.6)
project(apoll)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

add_executable(apoll crc32.c dynamic_resource.cpp hqsp.c main.cpp tcp_connection.cpp)
