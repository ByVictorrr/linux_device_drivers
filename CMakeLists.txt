cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

project("KERNEL_MODULES" VERSION 0.1.0 LANGUAGES C)
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)


# Include the kernel module helper functions

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
# You would need to create a FindKernelHeaders.cmake file in the cmake directory to use this
find_package(KernelHeaders REQUIRED)

# Definitions for building a kernel module
add_definitions(-D__KERNEL__ -DMODULE)



# Kernel configuration target
add_kernel_module(scull_main scull_main.c)
