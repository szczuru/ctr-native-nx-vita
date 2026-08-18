# switch/CMakeLists.txt (kopiowany z CMakeLists_switch.cmake przez workflow)
#
# KLUCZOWA ZMIANA W STOSUNKU DO POPRZEDNICH WERSJI:
# Kompilujemy upstream main.c (bez modyfikacji) jako główną jednostkę.
# main.c sam wciąga przez #include całą platformę i grę (unity build).
# Dodajemy tylko switch-specyficzne backendy platformy jako dodatkowe źródła.
#
# Wywoływany z:
#   cmake -B build-switch -S switch
#         -DCMAKE_TOOLCHAIN_FILE=switch/switch-toolchain.cmake
#         -DCTR_SOURCE_DIR=$(pwd)

cmake_minimum_required(VERSION 3.20)
project(ctr_native_switch C)
set(CMAKE_C_STANDARD 17)

if(NOT DEFINED CTR_SOURCE_DIR)
    set(CTR_SOURCE_DIR "${CMAKE_SOURCE_DIR}/..")
endif()
get_filename_component(CTR_SOURCE_DIR "${CTR_SOURCE_DIR}" ABSOLUTE)
message(STATUS "CTR source root: ${CTR_SOURCE_DIR}")

if(NOT DEFINED ENV{DEVKITPRO})
    message(FATAL_ERROR "DEVKITPRO environment variable is not set.")
endif()
set(DEVKITPRO $ENV{DEVKITPRO})
set(DEVKITA64 ${DEVKITPRO}/devkitA64)

# ── Flagi kompilacji ───────────────────────────────────────────────────────────
add_compile_options(
    -march=armv8-a+crc+crypto
    -mtune=cortex-a57
    -mtp=soft
    -fPIE
    -O2
    -ffunction-sections
    -fdata-sections
    -Wall
    -Wno-unused-function
    -Wno-strict-aliasing
    -DPLATFORM_SWITCH
    -D__SWITCH__
    -DCTR_NATIVE
    -DCTR_INTERNAL
    -DCTR_NATIVE_SKIP_BITWIDTH_CHECK=1
)

# ── Include paths ──────────────────────────────────────────────────────────────
include_directories(
    ${CTR_SOURCE_DIR}
    ${CTR_SOURCE_DIR}/include
    ${CTR_SOURCE_DIR}/platform
    ${CTR_SOURCE_DIR}/game
    ${CMAKE_SOURCE_DIR}
    ${DEVKITPRO}/libnx/include
    ${DEVKITPRO}/portlibs/switch/include
)

# ── Źródła ────────────────────────────────────────────────────────────────────
# Upstream main.c wciąga przez #include całą platformę i grę.
# Dodajemy tylko Switch-specyficzne backendy których upstream nie ma.
add_executable(ctr_native_switch.elf
    # Upstream entry point (unity include manifest – wciąga platform/ i game/)
    ${CTR_SOURCE_DIR}/main.c

    # Switch-specyficzne backendy platformy (nie wciągane przez upstream main.c)
    ${CTR_SOURCE_DIR}/platform/native_platform_switch.c
    ${CTR_SOURCE_DIR}/platform/native_renderer_switch.c
    ${CTR_SOURCE_DIR}/platform/native_libpad_switch.c
)

target_compile_definitions(ctr_native_switch.elf PRIVATE
    CTR_NATIVE_VERSION=\"${CTR_NATIVE_VERSION}\"
)

# ── Linker ────────────────────────────────────────────────────────────────────
target_link_options(ctr_native_switch.elf PRIVATE
    -march=armv8-a+crc+crypto
    -mtune=cortex-a57
    -mtp=soft
    -fPIE
    -Wl,--gc-sections
    -specs=${DEVKITPRO}/libnx/switch.specs
)

target_link_libraries(ctr_native_switch.elf
    ${DEVKITPRO}/libnx/lib/libnx.a
    m
    EGL
    glapi
    drm_nouveau
)

# ── NRO post-processing ───────────────────────────────────────────────────────
set(NRO_OUTPUT ${CMAKE_BINARY_DIR}/ctr_native.nro)
set(NACPFILE   ${CMAKE_BINARY_DIR}/ctr_native.nacp)
set(ICONFILE   ${CMAKE_SOURCE_DIR}/icon.jpg)

add_custom_command(
    OUTPUT ${NACPFILE}
    COMMAND ${DEVKITPRO}/tools/bin/nacptool
            --create "Crash Team Racing" "CTR-tools" "${CTR_NATIVE_VERSION}"
            ${NACPFILE}
    COMMENT "Generating NACP"
)

add_custom_command(
    OUTPUT ${NRO_OUTPUT}
    COMMAND ${DEVKITPRO}/tools/bin/elf2nro
            $<TARGET_FILE:ctr_native_switch.elf>
            ${NRO_OUTPUT}
            --nacp=${NACPFILE}
            $<$<BOOL:$<PATH:EXISTS,${ICONFILE}>>:--icon=${ICONFILE}>
    DEPENDS ctr_native_switch.elf ${NACPFILE}
    COMMENT "Generating NRO"
)

add_custom_target(nro ALL DEPENDS ${NRO_OUTPUT})
