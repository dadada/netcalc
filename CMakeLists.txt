cmake_minimum_required(VERSION 3.5)
project(netcalc)

set(CMAKE_C_STANDARD 11)
set(SOURCE_FILES netcalc.c)

add_executable(netcalc ${SOURCE_FILES})