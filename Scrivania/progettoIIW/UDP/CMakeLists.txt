cmake_minimum_required(VERSION 3.23)
project(UDP C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(UDP
        clientUDP.c
        common.h
        serverUDP.c common.c)
