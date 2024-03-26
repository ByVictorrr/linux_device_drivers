cmake_minimum_required(VERSION 3.10)
project(scull_access VERSION 1.0.0 LANGUAGES C)
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)


# You would need to create a FindKernelHeaders.cmake file in the cmake directory to use this
find_package(KernelHeaders REQUIRED)


# Kernel configuration target
add_kernel_module(${PROJECT_NAME} access.c ../scull.c)



# Parse c files
add_library(PHONY_${PROJECT_NAME} EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/access.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../scull.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../scull.h
        )
