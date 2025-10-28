cmake_minimum_required(VERSION 3.10)
project(Serwer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(serwer
    serwer.cpp
    macierz.cpp
    gauss.cpp
    gauss_process.cpp
)