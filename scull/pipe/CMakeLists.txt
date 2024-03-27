cmake_minimum_required(VERSION 3.10)
project(scull_pipe VERSION 1.0.0 LANGUAGES C)
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)


# You would need to create a FindKernelHeaders.cmake file in the cmake directory to use this
find_package(KernelHeaders REQUIRED)


# Kernel configuration target
add_kernel_module(${PROJECT_NAME} pipe.c ../scull.c)



# Parse c files
add_library(PHONY_${PROJECT_NAME} EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/pipe.c
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

set(CPACK_COMPONENT_PIPE_DISPLAY_NAME "${PROJECT_NAME}")
set(CPACK_COMPONENT_PIPE_DESCRIPTION "A kmod for scull_access.ko")
set(CPACK_DEBIAN_PIPE_PACKAGE_NAME "${PROJECT_NAME}")

# Include these lines in each subfolder's CMakeLists.txt
set(CPACK_COMPONENTS_ALL ${CPACK_COMPONENTS_ALL} access)
set(CPACK_COMPONENTS_GROUPING ONE_PER_GROUP)


set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_RELEASE 1)
set(CPACK_PACKAGE_SUMMARY "A device like pipe.")
set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
set(CPACK_PACKAGE_LICENSE "HPE")
SET(CPACK_PACKAGING_INSTALL_PREFIX "")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_BINARY_DIR}/postinst")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Victor Delaplaine")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "")
set(CPACK_RPM_PACKAGE_AUTOREQ "no") # Disable automatic dependency processing
set(CPACK_RPM_PACKAGE_REQUIRES "")
include(CPack)