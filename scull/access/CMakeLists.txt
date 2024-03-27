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

# Create custom target for uninstall
add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -E remove /lib/modules/${KERNEL_VERSION}/extra/${MODULE_NAME}.ko
        COMMAND ${CMAKE_COMMAND} -E remove /lib/postinst
        COMMENT "Uninstalling ${MODULE_NAME}"
        )

set(MODULE_NAME ${PROJECT_NAME})
set(DEVICE_NAME access)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../postinst.in ${CMAKE_CURRENT_BINARY_DIR}/postinst @ONLY)

# Set permissions for files
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}.ko  DESTINATION /lib/modules/${KERNEL_VERSION}/extra)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/postinst DESTINATION /lib/${MODULE_NAME})
# Run postinst script after installation
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E env KERNEL_VERSION=${KERNEL_VERSION} sh /lib/${MODULE_NAME}/postinst)")
set(CPACK_COMPONENT_FOLDERA_DISPLAY_NAME "Folder A Library")
set(CPACK_COMPONENT_FOLDERA_DESCRIPTION "A library from folder_a")
set(CPACK_DEBIAN_FOLDERA_PACKAGE_NAME "myproject-foldera")

# Include these lines in each subfolder's CMakeLists.txt
set(CPACK_COMPONENTS_ALL ${CPACK_COMPONENTS_ALL} access)
set(CPACK_COMPONENTS_GROUPING ONE_PER_GROUP)

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
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