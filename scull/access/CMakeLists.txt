cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

project("SCULL_PIPE" VERSION 0.1.0 LANGUAGES C)
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)


# You would need to create a FindKernelHeaders.cmake file in the cmake directory to use this
find_package(KernelHeaders REQUIRED)


# Kernel configuration target
add_kernel_module(scull_pipe
        ${CMAKE_CURRENT_SOURCE_DIR}/main.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../scull.c
        )

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ../scull.h DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_executable(_pipe_code_completion
        ${CMAKE_CURRENT_SOURCE_DIR}/main.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../scull.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../scull.h
        )

# Set up CPack to generate an RPM package
set(CPACK_GENERATOR "RPM")
set(CPACK_PACKAGE_NAME "scull_pipe")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_RELEASE 1)
set(CPACK_PACKAGE_SUMMARY "A device like pipe.")
set(CPACK_PACKAGE_LICENSE "HPE")
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_SOURCE_DIR}/rpm_postinstall.sh")
set(CPACK_PACKAGING_INSTALL_PREFIX ".")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}.noarch")

# Include the files in the package
install(FILES "${KERNEL_MODULE_SOURCE_DIR}/mymodule.ko"
        DESTINATION "lib/modules/$(uname -r)/extra"
        )

# Create the 'extra' directory if it doesn't exist
install(CODE "file(MAKE_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/lib/modules/\$(uname -r)/extra\")")

install(CODE "execute_process(COMMAND mknod -m 666 /dev/mydevice c 250 0)")

include(CPack)