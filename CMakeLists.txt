cmake_minimum_required(VERSION 3.10)
project(root_project)

# Include the kernel module helper functions
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
# You would need to create a FindKernelHeaders.cmake file in the cmake directory to use this

set(CPACK_TEMPORARY_DIRECTORY "/tmp")
# Add subdirectories
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/intro)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/scull)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/scullc)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/scullp)
# add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/misc-progs)