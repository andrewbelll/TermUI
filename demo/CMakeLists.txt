cmake_minimum_required(VERSION 3.10)
project(termui_demo VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(demo termui_demo.cpp)
target_include_directories(demo PRIVATE . ..)

add_executable(zip_demo termui_zip_demo.cpp)
target_compile_features(zip_demo PRIVATE cxx_std_11)
target_include_directories(zip_demo PRIVATE . ..)
