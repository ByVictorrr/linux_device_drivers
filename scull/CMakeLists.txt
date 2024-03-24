cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

project("SCULL_KERNEL_MODELS" VERSION 0.1.0 LANGUAGES C)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/scull_pipe)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/scull_access)

