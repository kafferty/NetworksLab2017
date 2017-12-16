cmake_minimum_required(VERSION 3.8.2)
project(udp)

set(CMAKE_CXX_STANDARD 11)
set(SOURCE_FILES main.cpp)
add_executable(udp main.cpp)

target_link_libraries(udp pthread)
