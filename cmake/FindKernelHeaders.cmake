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

set(KERNEL_SOURCE_DIR "/usr/src/${KERNEL_VERSION}")
# set(KERNEL_BUILD_DIR "/lib/modules/${KERNEL_VERSION}/build")
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

function(add_kernel_module MODULE_NAME)
    # Capture additional arguments as source files
    set(SOURCE_FILES ${ARGN})

    # Set the command to build the kernel module
    set(KBUILD_CMD $(MAKE) -C ${KERNEL_SOURCE_DIR} modules M=${CMAKE_CURRENT_BINARY_DIR} src=${CMAKE_CURRENT_SOURCE_DIR})

    # Generate the Kbuild file in the binary directory
    # Assuming SOURCE_FILES is a list of source files for the module
    set(OBJECT_FILES "")
    foreach(SOURCE_FILE IN LISTS SOURCE_FILES)
        string(REPLACE ".c" ".o" OBJECT_FILE "${SOURCE_FILE}")
        list(APPEND OBJECT_FILES "${OBJECT_FILE}")
    endforeach()
    string(REPLACE ";" " " OBJECT_FILES "${OBJECT_FILES}")
    message("obj files = ${OBJECT_FILES}")

    # Generate the Kbuild file with the correct object file list
    file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/Kbuild "obj-m := ${MODULE_NAME}.o\n")
    file(APPEND ${CMAKE_CURRENT_SOURCE_DIR}/Kbuild "${MODULE_NAME}-objs := ${OBJECT_FILES}\n")

    # Define a custom command to build the kernel module
    add_custom_command(OUTPUT ${MODULE_NAME}.o
            COMMAND sudo ${KBUILD_CMD}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            DEPENDS ${SOURCE_FILES} VERBATIM)

    # Define a custom target that depends on the output of the custom command
    add_custom_target(${MODULE_NAME}_driver ALL DEPENDS ${MODULE_NAME}.o)
endfunction()