cmake_minimum_required(VERSION 3.10)
project(root_project)

# Include the kernel module helper functions
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
# You would need to create a FindKernelHeaders.cmake file in the cmake directory to use this

# Add subdirectories
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ch2-building_running_modules)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/ch3-char_drivers)