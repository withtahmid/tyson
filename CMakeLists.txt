cmake_minimum_required(VERSION 3.16)
project(tyson CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(server
    src/main.cpp
)

target_compile_options(server PRIVATE -Wall -Wextra -g)
target_compile_options(server PRIVATE -fsanitize=address,undefined)
target_link_options(server PRIVATE -fsanitize=address,undefined)