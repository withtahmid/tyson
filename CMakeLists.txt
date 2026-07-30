cmake_minimum_required(VERSION 3.16)

project(tyson
    VERSION 0.1.0
    DESCRIPTION "An HTTP server written from scratch in C++"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

option(TYSON_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" ON)

add_executable(server
    apps/server.cpp
    src/error.cpp
    src/io.cpp
    src/listener.cpp
    src/connection.cpp
    src/request_reader.cpp        
    src/http.cpp   
)

target_include_directories(server PRIVATE include)

target_compile_options(server PRIVATE
    -Wall -Wextra -Wpedantic
    -Wshadow -Wconversion -Wsign-conversion
    -Wold-style-cast -Wcast-align
    -Wunused -Wdouble-promotion
)

if(TYSON_SANITIZE)
    target_compile_options(server PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(server PRIVATE -fsanitize=address,undefined)
endif()