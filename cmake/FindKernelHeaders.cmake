execute_process(
        COMMAND uname -r
        OUTPUT_VARIABLE KERNEL_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
# Include directories for kernel headers
# Add your include directories
include_directories(
        /usr/src/${KERNEL_VERSION}/include
        /usr/src/${KERNEL_VERSION}/arch/x86/include
)

# KernelModule.cmake
function(add_kernel_module MODULE_NAME SOURCE_FILES)
    # Determine the kernel version and set the kernel build directory
    execute_process(
            COMMAND uname -r
            OUTPUT_VARIABLE KERNEL_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(KERNEL_BUILD_DIR "/usr/src/${KERNEL_VERSION}")

    # Ensure the kernel build directory exists
    if(NOT EXISTS ${KERNEL_BUILD_DIR})
        message(FATAL_ERROR "Kernel build directory does not exist: ${KERNEL_BUILD_DIR}")
    endif()

    # Set the output path for the kernel module file
    set(MODULE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/${MODULE_NAME}.ko")

    # Generate the Makefile
    set(MAKEFILE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}/Makefile")
    file(WRITE ${MAKEFILE_PATH} "obj-m += ${MODULE_NAME}.o\n")
    file(APPEND ${MAKEFILE_PATH} "all:\n")
    file(APPEND ${MAKEFILE_PATH} "\t$(MAKE) -C ${KERNEL_BUILD_DIR} M=${CMAKE_CURRENT_BINARY_DIR} src=${CMAKE_CURRENT_SOURCE_DIR} modules\n")
    file(APPEND ${MAKEFILE_PATH} "clean:\n")
    file(APPEND ${MAKEFILE_PATH} "\t$(MAKE) -C ${KERNEL_BUILD_DIR} M=${CMAKE_CURRENT_BINARY_DIR} src=${CMAKE_CURRENT_SOURCE_DIR} clean\n")

    # Create the custom command and target for building the kernel module
    add_custom_command(
            OUTPUT ${MODULE_OUTPUT_PATH}
            COMMAND make -C ${KERNEL_BUILD_DIR} M=${CMAKE_CURRENT_BINARY_DIR} modules
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            DEPENDS ${SOURCE_FILES}
            COMMENT "Building kernel module ${MODULE_NAME}"
    )

    add_custom_target(
            ${MODULE_NAME}_module ALL
            DEPENDS ${MODULE_OUTPUT_PATH}
    )

    # Custom target for cleaning the kernel module
    add_custom_target(
            clean_${MODULE_NAME}_module
            COMMAND make -C ${KERNEL_BUILD_DIR} M=${CMAKE_CURRENT_BINARY_DIR} clean
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Cleaning kernel module ${MODULE_NAME}"
    )
endfunction()