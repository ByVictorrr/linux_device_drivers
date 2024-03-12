cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

project("misc-programs" VERSION 0.1.0 LANGUAGES C)
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(pipe_test ${CMAKE_CURRENT_SOURCE_DIR}/non_blocking_test.c)
