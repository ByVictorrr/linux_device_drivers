cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

project(scull VERSION 0.1.0 LANGUAGES C)
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)


# You would need to create a FindKernelHeaders.cmake file in the cmake directory to use this
find_package(KernelHeaders REQUIRED)


# Kernel configuration target
add_kernel_module(scull main.c ../scull.c)


# Parse c files
add_library(PHONY_${PROJECT_NAME} EXCLUDE_FROM_ALL
        main.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../scull.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../scull.h
        )

set(MODULE_NAME ${PROJECT_NAME})
SET(DEVICE_NAME pipe)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../postinst.in ${CMAKE_CURRENT_BINARY_DIR}/postinst @ONLY)

# Set permissions for files
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}.ko  DESTINATION /lib/modules/${KERNEL_VERSION}/extra)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/postinst DESTINATION /lib/${MODULE_NAME})
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E env KERNEL_VERSION=${KERNEL_VERSION} sh /lib/${MODULE_NAME}/postinst)")