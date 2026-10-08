cmake_minimum_required(VERSION 4.2)
project(os)

set(CMAKE_CXX_STANDARD 20)

add_executable(os main.cpp
        pcb.h
        pcb_queue.h
        process.h
        memory.h
        pcb_queue.cpp
        memory.cpp
        process.cpp
        bitmap.h
)
