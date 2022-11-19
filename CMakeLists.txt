cmake_minimum_required(VERSION 3.24)
project(gbn_arq_udp C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(
        gbn_arq_udp
        client.c
        gbn.h
        server.c
)
