cmake_minimum_required(VERSION 3.16)
project(MyGame LANGUAGES C CXX)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(raylib REQUIRED)

add_subdirectory(src)