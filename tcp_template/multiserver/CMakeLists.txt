cmake_minimum_required(VERSION 2.8)
project(multiserver)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -pthread")

set(SOURCE_FILES main.c)
add_executable(multiserver ${SOURCE_FILES})