cmake_minimum_required(VERSION 3.8)
project(testServer)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        bin/Debug/testServer.exe
        obj/Debug/main.o
        .gitignore.txt
        anstest1.txt
        list_tests.txt
        main.cpp
        main.exe
        main.o
        registered.txt
        test1.txt)

add_executable(testServer ${SOURCE_FILES})