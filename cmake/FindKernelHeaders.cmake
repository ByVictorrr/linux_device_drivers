# BIG THANK YOU TO THE ORIGINAL AUTHOR
# https://gitlab.com/christophacham/cmake-kernel-module

# Find the kernel release
execute_process(
        COMMAND uname -r
        OUTPUT_VARIABLE KERNEL_RELEASE
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Find the headers
# find_path(KERNELHEADERS_DIR PATHS /usr/src/linux/include)
set(KERNELHEADERS_DIR /usr/src/linux-headers-4.15.0-213-generic)
#include_directories(/usr/src/linux-headers-4.15.0-213-generic/include/)


message(STATUS "Kernel release: ${KERNEL_RELEASE}")
message(STATUS "Kernel headers: ${KERNELHEADERS_DIR}")

if (KERNELHEADERS_DIR)
    set(KERNELHEADERS_INCLUDE_DIRS
            ${KERNELHEADERS_DIR}/include
            ${KERNELHEADERS_DIR}/arch/x86/include
            CACHE PATH "Kernel headers include dirs"
            )
    set(KERNELHEADERS_FOUND 1 CACHE STRING "Set to 1 if kernel headers were found")
else (KERNELHEADERS_DIR)
    set(KERNELHEADERS_FOUND 0 CACHE STRING "Set to 1 if kernel headers were found")
endif (KERNELHEADERS_DIR)

mark_as_advanced(KERNELHEADERS_FOUND)
