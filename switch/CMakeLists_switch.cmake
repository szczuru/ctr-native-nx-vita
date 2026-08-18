# CMakeLists.txt dla Nintendo Switch (devkitPro / devkitA64 / libnx)
# Plik docelowy: switch/CMakeLists.txt
#
# WAŻNE: cmake wywoływany jest z -S switch, więc CMAKE_SOURCE_DIR = switch/
# Katalog główny repozytorium przekazywany jest przez -DCTR_SOURCE_DIR=<root>
#
# Użycie:
#   cmake -B build-switch -S switch \
#         -DCMAKE_TOOLCHAIN_FILE=switch/switch-toolchain.cmake \
#         -DCTR_SOURCE_DIR=$(pwd)

cmake_minimum_required(VERSION 3.20)
project(ctr_native_switch C)
set(CMAKE_C_STANDARD 17)

if(NOT DEFINED CTR_SOURCE_DIR)
    # Fallback: jeśli ktoś uruchamia cmake z katalogu głównego
    set(CTR_SOURCE_DIR "${CMAKE_SOURCE_DIR}/..")
endif()

# Upewnij się że mamy absolutną ścieżkę
get_filename_component(CTR_SOURCE_DIR "${CTR_SOURCE_DIR}" ABSOLUTE)

message(STATUS "CTR source root: ${CTR_SOURCE_DIR}")

# ── Środowisko devkitPro ───────────────────────────────────────────────────────
if(NOT DEFINED ENV{DEVKITPRO})
    message(FATAL_ERROR "DEVKITPRO environment variable is not set.")
endif()
set(DEVKITPRO $ENV{DEVKITPRO})
set(DEVKITA64 ${DEVKITPRO}/devkitA64)

# ── Flagi kompilacji ───────────────────────────────────────────────────────────
set(SWITCH_ARCH_FLAGS "-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE")

add_compile_options(
    -O2
    -ffunction-sections
    -fdata-sections
    -Wall
    -Wno-unused-function
    -Wno-strict-aliasing
    # Identyfikacja platformy
    -DPLATFORM_SWITCH
    -D__SWITCH__
    -DCTR_NATIVE
    -DCTR_INTERNAL
    # Pomiń sprawdzenie 32-bit (Switch jest AArch64)
    -DCTR_NATIVE_SKIP_BITWIDTH_CHECK=1
)

separate_arguments(SWITCH_ARCH_LIST UNIX_COMMAND "${SWITCH_ARCH_FLAGS}")
add_compile_options(${SWITCH_ARCH_LIST})

# ── Include paths ──────────────────────────────────────────────────────────────
include_directories(
    ${CTR_SOURCE_DIR}
    ${CTR_SOURCE_DIR}/include
    ${CTR_SOURCE_DIR}/platform
    ${CMAKE_SOURCE_DIR}          # switch/ – nagłówki specyficzne dla platformy
    ${DEVKITPRO}/libnx/include
    ${DEVKITPRO}/portlibs/switch/include
)

# ── Źródła ────────────────────────────────────────────────────────────────────
# Switch entry point (zastępuje główny main.c)
set(SWITCH_SOURCES
    ${CMAKE_SOURCE_DIR}/main_switch.c

    # Warstwa platformowa Switch
    ${CTR_SOURCE_DIR}/platform/native_platform_switch.c
    ${CTR_SOURCE_DIR}/platform/native_renderer_switch.c
    ${CTR_SOURCE_DIR}/platform/native_libpad_switch.c

    # Upstream platform (niezmienione)
    ${CTR_SOURCE_DIR}/platform/native_assets.c
    ${CTR_SOURCE_DIR}/platform/native_audio.c
    ${CTR_SOURCE_DIR}/platform/native_memory.c
    ${CTR_SOURCE_DIR}/platform/native_checkpoint.c
    ${CTR_SOURCE_DIR}/platform/native_checkpoint_file.c
    ${CTR_SOURCE_DIR}/platform/native_cd.c
    ${CTR_SOURCE_DIR}/platform/native_disc_image.c
    ${CTR_SOURCE_DIR}/platform/native_gpu_links.c
    ${CTR_SOURCE_DIR}/platform/native_gpu.c
    ${CTR_SOURCE_DIR}/platform/native_gte_core.c
    ${CTR_SOURCE_DIR}/platform/native_inline_c.c
    ${CTR_SOURCE_DIR}/platform/native_libapi.c
    ${CTR_SOURCE_DIR}/platform/native_libetc.c
    ${CTR_SOURCE_DIR}/platform/native_libgte.c
    ${CTR_SOURCE_DIR}/platform/native_libgpu.c
    ${CTR_SOURCE_DIR}/platform/native_libspu.c
    ${CTR_SOURCE_DIR}/platform/native_log.c
    ${CTR_SOURCE_DIR}/platform/native_memcard.c
    ${CTR_SOURCE_DIR}/platform/native_memcard_adapter.c
    ${CTR_SOURCE_DIR}/platform/native_perf.c
    ${CTR_SOURCE_DIR}/platform/native_platform.c
    ${CTR_SOURCE_DIR}/platform/native_replay_scheduler.c
    ${CTR_SOURCE_DIR}/platform/native_renderer.c
    ${CTR_SOURCE_DIR}/platform/native_savestate.c
    ${CTR_SOURCE_DIR}/platform/native_state.c
    ${CTR_SOURCE_DIR}/platform/native_str.c

    # Kod gry (unity include)
    ${CTR_SOURCE_DIR}/game/game_unity.c
    ${CTR_SOURCE_DIR}/game/zGlobal_RDATA.c
    ${CTR_SOURCE_DIR}/game/zGlobal_DATA.c
    ${CTR_SOURCE_DIR}/game/zGlobal_SDATA.c
)

add_executable(ctr_native_switch.elf ${SWITCH_SOURCES})

target_compile_definitions(ctr_native_switch.elf PRIVATE
    CTR_NATIVE_VERSION=\"${CTR_NATIVE_VERSION}\"
)

# ── Linker ────────────────────────────────────────────────────────────────────
target_link_options(ctr_native_switch.elf PRIVATE
    ${SWITCH_ARCH_LIST}
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
set(NRO_OUTPUT   ${CMAKE_BINARY_DIR}/ctr_native.nro)
set(ELF_OUTPUT   $<TARGET_FILE:ctr_native_switch.elf>)
set(NACPFILE     ${CMAKE_BINARY_DIR}/ctr_native.nacp)
set(ICONFILE     ${CMAKE_SOURCE_DIR}/icon.jpg)

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
            ${ELF_OUTPUT}
            ${NRO_OUTPUT}
            --nacp=${NACPFILE}
            $<$<BOOL:${ICONFILE}>:--icon=${ICONFILE}>
    DEPENDS ctr_native_switch.elf ${NACPFILE}
    COMMENT "Generating NRO"
)

add_custom_target(nro ALL DEPENDS ${NRO_OUTPUT})
