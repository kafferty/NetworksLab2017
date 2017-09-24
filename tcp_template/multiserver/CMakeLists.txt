cmake_minimum_required(VERSION 3.8)
project(multiserver)

set(CMAKE_C_STANDARD 11)

set(SOURCE_FILES main.c)
add_executable(multiserver ${SOURCE_FILES})
target_link_libraries(multiserver wsock32)
