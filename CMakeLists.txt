cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

project("Linux Kernel Module with CLion IDE support / CMake" VERSION 0.1.0 LANGUAGES C)
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find kernel headers
# list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
# You would need to create a FindKernelHeaders.cmake file in the cmake directory to use this
# find_package(KernelHeaders REQUIRED)

# Definitions for building a kernel module
add_definitions(-D__KERNEL__ -DMODULE)

# Include directories for kernel headers
# Add your include directories
include_directories(
        /usr/src/linux-headers-4.15.0-213/include
        /usr/src/linux-headers-4.15.0-213/arch/x86/include
)
# Kernel module target
add_library(char_mod MODULE
        # Add all *.h and *.c files here that CLion should cover
        "${CMAKE_SOURCE_DIR}/scull_main.c"
)

# Ensure we're not linking against any libs
set_target_properties(char_mod PROPERTIES LINKER_LANGUAGE C)