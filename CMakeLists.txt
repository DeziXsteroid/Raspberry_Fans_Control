cmake_minimum_required(VERSION 3.10)
project(Raspberry_Fun_Control)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)
find_package(Threads REQUIRED)

add_executable(Raspberry_Fun_Control
        main.cpp
        AppInfo.h
        AppInfo.cpp
        Temperature_Sys.cpp
        Temperature_Sys.h
        Paths_Sys.h
        Status_Sys.h
        Status_Sys.cpp
        FanControl_Sys.h
        FanControl_Sys.cpp
)

target_link_libraries(Raspberry_Fun_Control PRIVATE Threads::Threads)
