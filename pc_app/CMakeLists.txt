cmake_minimum_required(VERSION 3.10)
project(ReverbPC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(reverb_pc main.cpp reverb.cpp)
add_executable(test_reverb test_reverb.cpp reverb.cpp)
