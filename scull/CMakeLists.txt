cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

project("Chapter3 - Char Drivers" VERSION 0.1.0 LANGUAGES C)
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)


# You would need to create a FindKernelHeaders.cmake file in the cmake directory to use this
find_package(KernelHeaders REQUIRED)


# Kernel configuration target
add_kernel_module(char_scull_main
        src/scull.c
        src/pipe.c
        src/access.c
        src/main.c
        )


add_executable(_pipe
        src/pipe.c
        ${CMAKE_CURRENT_SOURCE_DIR}/include/scull.h
        src/access.c
        src/main.c

        )
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/scull_actions.sh DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/char_scull_main)

add_custom_target(init_char
        COMMAND sudo ./scull_actions.sh
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/char_scull_main"
        DEPENDS char_scull_main
        COMMENT "Building kernel module ${MODULE_NAME}"
)