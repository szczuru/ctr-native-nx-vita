# switch-toolchain.cmake
# CMake cross-compilation toolchain for Nintendo Switch (devkitA64 / libnx)
#
# Pass to cmake with:
#   cmake -DCMAKE_TOOLCHAIN_FILE=switch/switch-toolchain.cmake ...

if(NOT DEFINED ENV{DEVKITPRO})
    message(FATAL_ERROR "DEVKITPRO environment variable is not set.")
endif()

set(DEVKITPRO $ENV{DEVKITPRO})
set(DEVKITA64 ${DEVKITPRO}/devkitA64)

# Target system
set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Compiler executables
set(CMAKE_C_COMPILER   ${DEVKITA64}/bin/aarch64-none-elf-gcc   CACHE PATH "C compiler")
set(CMAKE_CXX_COMPILER ${DEVKITA64}/bin/aarch64-none-elf-g++   CACHE PATH "C++ compiler")
set(CMAKE_ASM_COMPILER ${DEVKITA64}/bin/aarch64-none-elf-gcc   CACHE PATH "ASM compiler")
set(CMAKE_AR           ${DEVKITA64}/bin/aarch64-none-elf-ar     CACHE PATH "Archiver")
set(CMAKE_RANLIB       ${DEVKITA64}/bin/aarch64-none-elf-ranlib CACHE PATH "Ranlib")
set(CMAKE_STRIP        ${DEVKITA64}/bin/aarch64-none-elf-strip  CACHE PATH "Strip")
set(CMAKE_OBJCOPY      ${DEVKITA64}/bin/aarch64-none-elf-objcopy CACHE PATH "Objcopy")

# Don't try to find host libraries
set(CMAKE_FIND_ROOT_PATH
    ${DEVKITA64}/aarch64-none-elf
    ${DEVKITPRO}/libnx
    ${DEVKITPRO}/portlibs/switch
)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Suppress "cannot find runtime library" noise during CMake test compile
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
