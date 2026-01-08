cmake_minimum_required(VERSION 3.10)

project(ssd_cafeteria_congestion VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "-std=c++23")


find_package(OpenSSL REQUIRED)
add_definitions(-DCROW_ENABLE_SSL)
include_directories(${OPENSSL_INCLUDE_DIR})


include_directories(include/Crow)
add_subdirectory(src)



