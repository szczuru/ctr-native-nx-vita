# switch-toolchain.cmake
# CMake cross-compilation toolchain dla Nintendo Switch (devkitA64 / libnx).
#
# UWAGA: ctr-native wymaga 32-bit build (sizeof(void*)==4).
# Switch AArch64 jest 64-bit, dlatego:
#   1. Przekazujemy -DCTR_NATIVE_SKIP_BITWIDTH_CHECK=1 w workflow
#   2. Kompilujemy z flagami ILP32 gdzie to możliwe, lub akceptujemy
#      64-bit i polegamy na native_gpu_links.c do obsługi PSX primitive links.
#
# Użycie:
#   cmake -B build-switch \
#         -DCMAKE_TOOLCHAIN_FILE=switch/switch-toolchain.cmake \
#         -DPLATFORM=switch \
#         -DCTR_NATIVE_SKIP_BITWIDTH_CHECK=1

if(NOT DEFINED ENV{DEVKITPRO})
    message(FATAL_ERROR "DEVKITPRO environment variable is not set.")
endif()

set(DEVKITPRO $ENV{DEVKITPRO})
set(DEVKITA64 ${DEVKITPRO}/devkitA64)

# Target system
set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Kompilatory AArch64 (devkitA64)
set(CMAKE_C_COMPILER   ${DEVKITA64}/bin/aarch64-none-elf-gcc   CACHE PATH "C compiler")
set(CMAKE_CXX_COMPILER ${DEVKITA64}/bin/aarch64-none-elf-g++   CACHE PATH "C++ compiler")
set(CMAKE_ASM_COMPILER ${DEVKITA64}/bin/aarch64-none-elf-gcc   CACHE PATH "ASM compiler")
set(CMAKE_AR           ${DEVKITA64}/bin/aarch64-none-elf-ar     CACHE PATH "Archiver")
set(CMAKE_RANLIB       ${DEVKITA64}/bin/aarch64-none-elf-ranlib CACHE PATH "Ranlib")
set(CMAKE_STRIP        ${DEVKITA64}/bin/aarch64-none-elf-strip  CACHE PATH "Strip")
set(CMAKE_OBJCOPY      ${DEVKITA64}/bin/aarch64-none-elf-objcopy CACHE PATH "Objcopy")

# Ścieżki sysroot
set(CMAKE_FIND_ROOT_PATH
    ${DEVKITA64}/aarch64-none-elf
    ${DEVKITPRO}/libnx
    ${DEVKITPRO}/portlibs/switch
)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Unikamy błędnego testu kompilatora przy CMake try_compile
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Flagi architekturalne Switch (AArch64 Cortex-A57)
set(SWITCH_ARCH_FLAGS
    "-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE"
)
set(CMAKE_C_FLAGS_INIT   "${SWITCH_ARCH_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "${SWITCH_ARCH_FLAGS} -fPIE -Wl,--gc-sections -specs=${DEVKITPRO}/libnx/switch.specs"
)

# Definicje platformy – widoczne w całym projekcie
add_compile_definitions(
    PLATFORM_SWITCH
    __SWITCH__
)

# Pomijamy wymóg 32-bit – Switch używa własnej warstwy abstrakcji PSX primitive links
set(CTR_NATIVE_SKIP_BITWIDTH_CHECK 1 CACHE BOOL "Skip 32-bit pointer size check for Switch port" FORCE)
