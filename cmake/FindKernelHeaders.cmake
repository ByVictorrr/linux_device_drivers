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

    # Define the module build directory within the binary directory
    set(MODULE_DIR "${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}")
    file(MAKE_DIRECTORY ${MODULE_DIR})

    # Copy source files to the module build directory
    foreach(SOURCE_FILE ${SOURCE_FILES})
        get_filename_component(SRC_FILE_NAME ${SOURCE_FILE} NAME)
        get_filename_component(SRC_FILE_DIR ${SOURCE_FILE} DIRECTORY)
        file(COPY ${SOURCE_FILE} DESTINATION ${MODULE_DIR})
    endforeach()

    # Generate the Makefile
    set(MAKEFILE_PATH "${MODULE_DIR}/Makefile")
    file(WRITE ${MAKEFILE_PATH} "obj-m += ${MODULE_NAME}.o\n")
    file(APPEND ${MAKEFILE_PATH} "all:\n")
    file(APPEND ${MAKEFILE_PATH} "\t$(MAKE) -C ${KERNEL_BUILD_DIR} M=${MODULE_DIR} modules\n")
    file(APPEND ${MAKEFILE_PATH} "clean:\n")
    file(APPEND ${MAKEFILE_PATH} "\t$(MAKE) -C ${KERNEL_BUILD_DIR} M=${MODULE_DIR} clean\n")

    # Create the custom command and target for building the kernel module
    add_custom_command(
            OUTPUT ${MODULE_DIR}/${MODULE_NAME}.ko
            COMMAND make -C ${KERNEL_BUILD_DIR} M=${MODULE_DIR} modules
            WORKING_DIRECTORY ${MODULE_DIR}
            DEPENDS ${SOURCE_FILES}
            COMMENT "Building kernel module ${MODULE_NAME}"
    )

    add_custom_target(
            ${MODULE_NAME}_module ALL
            DEPENDS ${MODULE_DIR}/${MODULE_NAME}.ko
    )

    # Custom target for cleaning the kernel module
    add_custom_target(
            clean_${MODULE_NAME}_module
            COMMAND make -C ${KERNEL_BUILD_DIR} M=${MODULE_DIR} clean
            WORKING_DIRECTORY ${MODULE_DIR}
            COMMENT "Cleaning kernel module ${MODULE_NAME}"
    )
endfunction()
