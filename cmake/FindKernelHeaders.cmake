execute_process(
        COMMAND uname -r
        OUTPUT_VARIABLE KERNEL_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
add_definitions(-D__KERNEL__ -DMODULE)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nostdinc")

# -------- extract c include paths -----------
# Define a variable to store the include paths
set(GCC_INCLUDE_PATHS "")
# Execute the command to get GCC system include paths
execute_process(
        COMMAND bash -c "${CMAKE_C_COMPILER} -xc++ -E -v - </dev/null 2>&1 | grep '^ ' | grep '/usr.*include'"
        OUTPUT_VARIABLE GCC_INCLUDE_PATHS_RAW
)
# Split the raw output into a list
string(REPLACE "\n" ";" GCC_INCLUDE_PATHS_LIST "${GCC_INCLUDE_PATHS_RAW}")
# Remove leading and trailing whitespaces from each path
foreach(PATH IN LISTS GCC_INCLUDE_PATHS_LIST)
    string(STRIP "${PATH}" TRIMMED_PATH)
    list(APPEND GCC_INCLUDE_PATHS "${TRIMMED_PATH}")
endforeach()
# Print the include paths
foreach(INCLUDE_PATH ${GCC_INCLUDE_PATHS})
    message(STATUS "GCC include path: ${INCLUDE_PATH}")
endforeach()

set(KERNEL_BUILD_DIR "/usr/src/${KERNEL_VERSION}")
include_directories(
        /usr/src/${KERNEL_VERSION}/include
        /usr/src/${KERNEL_VERSION}/include/uapi
        /usr/src/${KERNEL_VERSION}/include/asm-generic
        /usr/src/${KERNEL_VERSION}/include/generated/uapi
        /usr/src/${KERNEL_VERSION}/arch/x86/include
        /usr/src/${KERNEL_VERSION}/arch/x86/include/uapi
        /usr/src/${KERNEL_VERSION}/arch/x86/include/generated
        /usr/src/${KERNEL_VERSION}/arch/x86/include/generated/uapi
        /usr/src/${KERNEL_VERSION}/arch/x86/include/generated/asm
        /usr/src/${KERNEL_VERSION}/arch/x86/include/uapi/asm
        /usr/src/${KERNEL_VERSION}/arch/x86/include/asm
        /usr/src/${KERNEL_VERSION}/arch/x86/um/asm
        "${GCC_INCLUDE_PATHS}"
)


# KernelModule.cmake
function(add_kernel_module MODULE_NAME SOURCE_FILES)
    # Determine the kernel version and set the kernel build directory

    message(INFO "${KERNEL_BUILD_DIR}")
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
        set(DEST_FILE_PATH "${MODULE_DIR}/${SRC_FILE_NAME}")

        # Custom command to copy and generate build rule
        add_custom_command(
                OUTPUT ${DEST_FILE_PATH}
                COMMAND ${CMAKE_COMMAND} -E copy ${SOURCE_FILE} ${DEST_FILE_PATH}
                DEPENDS ${SOURCE_FILE}
                COMMENT "Copying ${SOURCE_FILE} to ${DEST_FILE_PATH}"
        )

        # Add the destination file to a list
        list(APPEND DEST_FILES ${DEST_FILE_PATH})
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
            ${MODULE_NAME}
            DEPENDS ${MODULE_DIR}/${MODULE_NAME}.ko
            SOURCES ${SOURCE_FILES} ${DEST_FILES}
    )

    # Custom target for cleaning the kernel module
    add_custom_target(
            clean_${MODULE_NAME}_module
            COMMAND make -C ${KERNEL_BUILD_DIR} M=${MODULE_DIR} clean
            WORKING_DIRECTORY ${MODULE_DIR}
            COMMENT "Cleaning kernel module ${MODULE_NAME}"
    )

    # Custom target for inserting the kernel module using insmod
    add_custom_target(
            insert_${MODULE_NAME}_module
            COMMAND sudo insmod ${MODULE_DIR}/${MODULE_NAME}.ko
            DEPENDS ${MODULE_DIR}/${MODULE_NAME}.ko
            COMMENT "Inserting kernel module ${MODULE_NAME}"
    )

    # Custom target for removing the kernel module using rmmod
    add_custom_target(
            remove_${MODULE_NAME}_module
            COMMAND sudo rmmod ${MODULE_DIR}/${MODULE_NAME}.ko
            COMMENT "Removing kernel module ${MODULE_NAME}"
    )
    add_executable(_headers_${MODULE_NAME}  ${SOURCE_FILES})
endfunction()