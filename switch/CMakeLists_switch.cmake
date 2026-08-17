# ─────────────────────────────────────────────────────────────────────────────
# CTR Native – Nintendo Switch (devkitPro / libnx) CMake build
#
# Prerequisites:
#   - devkitPro with devkitA64 and libnx installed
#   - devkitpro-pacman packages: devkitA64, libnx, switch-tools, switch-sdl3
#   - DEVKITPRO environment variable set (usually /opt/devkitpro)
#
# Usage:
#   cmake -B build-switch -DCMAKE_TOOLCHAIN_FILE=switch/switch-toolchain.cmake \
#         -DPLATFORM=switch
#   cmake --build build-switch
#
# Output: ctr_native.nro  (copy to /switch/ctr_native/ on SD card)
# ─────────────────────────────────────────────────────────────────────────────

cmake_minimum_required(VERSION 3.20)

# ── Environment ───────────────────────────────────────────────────────────────
if(NOT DEFINED ENV{DEVKITPRO})
    message(FATAL_ERROR
        "DEVKITPRO is not set.\n"
        "Install devkitPro and set the environment variable, e.g.:\n"
        "  export DEVKITPRO=/opt/devkitpro")
endif()

set(DEVKITPRO $ENV{DEVKITPRO})
set(DEVKITA64 ${DEVKITPRO}/devkitA64)

# ── Toolchain (also handled by switch-toolchain.cmake, listed here for IDE) ──
set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER   ${DEVKITA64}/bin/aarch64-none-elf-gcc)
set(CMAKE_CXX_COMPILER ${DEVKITA64}/bin/aarch64-none-elf-g++)
set(CMAKE_AR           ${DEVKITA64}/bin/aarch64-none-elf-ar)
set(CMAKE_RANLIB       ${DEVKITA64}/bin/aarch64-none-elf-ranlib)
set(CMAKE_STRIP        ${DEVKITA64}/bin/aarch64-none-elf-strip)

project(ctr_native_switch C)
set(CMAKE_C_STANDARD 17)

# ── libnx / devkitPro paths ───────────────────────────────────────────────────
set(LIBNX_DIR        ${DEVKITPRO}/libnx)
set(PORTLIBS_DIR     ${DEVKITPRO}/portlibs/switch)
set(SWITCH_TOOLS_DIR ${DEVKITPRO}/tools/bin)

# ── Compiler flags ────────────────────────────────────────────────────────────
set(ARCH_FLAGS
    -march=armv8-a+crc+crypto
    -mtune=cortex-a57
    -mtp=soft
    -fPIE
)

add_compile_options(
    ${ARCH_FLAGS}
    -O2
    -ffunction-sections
    -fdata-sections
    -Wall -Wno-unused-function -Wno-strict-aliasing

    # Platform identification
    -DPLATFORM_SWITCH
    -D__SWITCH__

    # Feature flags (edit to taste or expose as cmake options)
    -DUSE_16BY9=0          # default: 4:3 pillarbox; user changes via config.ini
    -DTARGET_FPS=30        # default: 30; user changes via config.ini

    # SDL3 EGL / OpenGL ES
    -DSDL_VIDEO_OPENGL_ES2=1
)

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/platform
    ${CMAKE_SOURCE_DIR}/switch
    ${LIBNX_DIR}/include
    ${PORTLIBS_DIR}/include
)

# ── Source files ──────────────────────────────────────────────────────────────
# Start from the same unity-include structure as the PC build,
# but swap main.c for main_switch.c and add switch-specific platform files.

set(SWITCH_SOURCES
    # Switch entry point (replaces upstream main.c)
    ${CMAKE_SOURCE_DIR}/switch/main_switch.c

    # Switch platform backends
    ${CMAKE_SOURCE_DIR}/platform/native_platform_switch.c
    ${CMAKE_SOURCE_DIR}/platform/native_renderer_switch.c
    ${CMAKE_SOURCE_DIR}/platform/native_libpad_switch.c

    # Upstream platform layer (audio, CD, assets, etc. – unchanged)
    ${CMAKE_SOURCE_DIR}/platform/native_assets.c
    ${CMAKE_SOURCE_DIR}/platform/native_audio.c
    ${CMAKE_SOURCE_DIR}/platform/native_memory.c
    ${CMAKE_SOURCE_DIR}/platform/native_checkpoint.c
    ${CMAKE_SOURCE_DIR}/platform/native_checkpoint_file.c
    ${CMAKE_SOURCE_DIR}/platform/native_cd.c
    ${CMAKE_SOURCE_DIR}/platform/native_disc_image.c
    ${CMAKE_SOURCE_DIR}/platform/native_gpu_links.c
    ${CMAKE_SOURCE_DIR}/platform/native_gpu.c
    ${CMAKE_SOURCE_DIR}/platform/native_gte_core.c
    ${CMAKE_SOURCE_DIR}/platform/native_inline_c.c
    ${CMAKE_SOURCE_DIR}/platform/native_libapi.c
    ${CMAKE_SOURCE_DIR}/platform/native_libetc.c
    ${CMAKE_SOURCE_DIR}/platform/native_libgte.c
    ${CMAKE_SOURCE_DIR}/platform/native_libgpu.c
    ${CMAKE_SOURCE_DIR}/platform/native_libspu.c
    ${CMAKE_SOURCE_DIR}/platform/native_log.c
    ${CMAKE_SOURCE_DIR}/platform/native_memcard.c
    ${CMAKE_SOURCE_DIR}/platform/native_memcard_adapter.c
    ${CMAKE_SOURCE_DIR}/platform/native_perf.c
    ${CMAKE_SOURCE_DIR}/platform/native_platform.c
    ${CMAKE_SOURCE_DIR}/platform/native_replay_scheduler.c
    ${CMAKE_SOURCE_DIR}/platform/native_renderer.c
    ${CMAKE_SOURCE_DIR}/platform/native_savestate.c
    ${CMAKE_SOURCE_DIR}/platform/native_state.c
    ${CMAKE_SOURCE_DIR}/platform/native_str.c

    # Game source (unchanged – all 943 files via unity include)
    ${CMAKE_SOURCE_DIR}/game/game_unity.c
    ${CMAKE_SOURCE_DIR}/game/zGlobal_RDATA.c
    ${CMAKE_SOURCE_DIR}/game/zGlobal_DATA.c
    ${CMAKE_SOURCE_DIR}/game/zGlobal_SDATA.c
)

add_executable(ctr_native_switch.elf ${SWITCH_SOURCES})

# ── SDL3 (from portlibs/switch) ───────────────────────────────────────────────
# devkitpro-pacman: switch-sdl3
find_library(SDL3_LIB SDL3 PATHS ${PORTLIBS_DIR}/lib REQUIRED)

# ── Linker ────────────────────────────────────────────────────────────────────
set(LINK_FLAGS
    ${ARCH_FLAGS}
    -fPIE
    -Wl,--gc-sections
    -specs=${DEVKITPRO}/libnx/switch.specs
)

target_link_options(ctr_native_switch.elf PRIVATE ${LINK_FLAGS})

target_link_libraries(ctr_native_switch.elf
    ${SDL3_LIB}
    ${LIBNX_DIR}/lib/libnx.a
    ${DEVKITA64}/lib/gcc/aarch64-none-elf/*/libstdc++.a
    m
    EGL
    glapi
    drm_nouveau
)

# ── NRO post-processing ───────────────────────────────────────────────────────
set(NRO_OUTPUT ${CMAKE_BINARY_DIR}/ctr_native.nro)
set(ELF_OUTPUT $<TARGET_FILE:ctr_native_switch.elf>)
set(NACPFILE   ${CMAKE_SOURCE_DIR}/switch/ctr_native.nacp)
set(ICONFILE   ${CMAKE_SOURCE_DIR}/switch/icon.jpg)
set(ROMFSDIR   "")   # set to a directory path to embed romfs assets in NRO

if(EXISTS ${ROMFSDIR})
    set(ROMFS_ARG --romfsdir=${ROMFSDIR})
else()
    set(ROMFS_ARG "")
endif()

add_custom_command(
    OUTPUT ${NRO_OUTPUT}
    COMMAND ${SWITCH_TOOLS_DIR}/nacptool
            --create "Crash Team Racing" "CTR-tools" "${CTR_NATIVE_VERSION:-dev}"
            ${NACPFILE}
    COMMAND ${SWITCH_TOOLS_DIR}/elf2nro
            ${ELF_OUTPUT} ${NRO_OUTPUT}
            --nacp=${NACPFILE}
            --icon=${ICONFILE}
            ${ROMFS_ARG}
    DEPENDS ctr_native_switch.elf
    COMMENT "Generating NRO homebrew package"
)

add_custom_target(nro ALL DEPENDS ${NRO_OUTPUT})

# ── Install target (copies NRO to switch/ subdirectory for SD card) ───────────
install(FILES ${NRO_OUTPUT}
        DESTINATION switch/ctr_native)
