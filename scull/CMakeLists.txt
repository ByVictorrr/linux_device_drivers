cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

project("SCULL_KERNEL_MODELS" VERSION 0.1.0 LANGUAGES C)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/pipe)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/access)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/scull)

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_CONTACT "Your Name <your.email@example.com>")
include(CPack)
