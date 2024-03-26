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


# Set permissions for files
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}.ko  DESTINATION /lib/modules/${KERNEL_VERSION}/extra)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/postinst DESTINATION /lib)

# Set up CPack to generate an RPM package
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
# set(CPACK_GENERATOR "RPM;DEB")
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_RELEASE 1)
set(CPACK_PACKAGE_SUMMARY "A device like pipe.")
set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
set(CPACK_PACKAGE_LICENSE "HPE")
SET(CPACK_PACKAGING_INSTALL_PREFIX "")
# set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/rpm_postinstall.sh")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/postinst")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Victor Delaplaine")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "")

# set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}.noarch")
set(CPACK_RPM_PACKAGE_AUTOREQ "no") # Disable automatic dependency processing

set(CPACK_RPM_PACKAGE_REQUIRES "")
# Create the 'extra' directory if it doesn't exist
include(CPack)