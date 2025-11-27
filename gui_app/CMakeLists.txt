cmake_minimum_required(VERSION 3.10)
project(ReverbGUI)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(wxWidgets REQUIRED)
include(${wxWidgets_USE_FILE})

find_package(PkgConfig REQUIRED)
pkg_check_modules(RTAUDIO REQUIRED rtaudio)

include_directories(${RTAUDIO_INCLUDE_DIRS})
include_directories(../third_party)

add_executable(reverb_gui main.cpp)
target_link_libraries(reverb_gui ${wxWidgets_LIBRARIES} ${RTAUDIO_LIBRARIES} -lpthread reverb_lib)
