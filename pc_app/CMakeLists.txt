cmake_minimum_required(VERSION 3.10)
project(ReverbPC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(../third_party)

add_executable(reverb_pc main.cpp)
target_link_libraries(reverb_pc reverb_lib)

add_executable(test_reverb test_reverb.cpp)
target_link_libraries(test_reverb reverb_lib)
