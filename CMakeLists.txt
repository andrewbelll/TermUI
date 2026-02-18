cmake_minimum_required(VERSION 3.10)
project(termui_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(demo termui_demo.cpp)
