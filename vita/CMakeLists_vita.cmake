# ─────────────────────────────────────────────────────────────────────────────
# CTR Native – PlayStation Vita (vitasdk) CMake build
#
# Prerequisites:
#   - vitasdk installed (https://vitasdk.org/)
#   - VITASDK environment variable set (usually /usr/local/vitasdk)
#   - vitaGL installed to vitasdk sysroot
#   - cmake-vita toolchain file (provided by vitasdk)
#
# Usage:
#   cmake -B build-vita \
#         -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake" \
#         -DPLATFORM=vita
#   cmake --build build-vita
#
# Output: ctr_native.vpk  (install to Vita via VitaShell or FTP)
# ─────────────────────────────────────────────────────────────────────────────

cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED ENV{VITASDK})
    message(FATAL_ERROR
        "VITASDK is not set.\n"
        "Install vitasdk from https://vitasdk.org and set the variable:\n"
        "  export VITASDK=/usr/local/vitasdk")
endif()

set(VITASDK $ENV{VITASDK})

project(ctr_native_vita C)
set(CMAKE_C_STANDARD 17)

# ── Compiler flags ────────────────────────────────────────────────────────────
add_compile_options(
    -O2
    -ffunction-sections
    -fdata-sections
    -Wall -Wno-unused-function -Wno-strict-aliasing

    -DPLATFORM_VITA
    -DUSE_16BY9=0     # default off; user enables via config.ini
    -DTARGET_FPS=30   # default 30; user enables via config.ini

    # ARM cortex-a9 (Vita CPU)
    -mcpu=cortex-a9 -mfpu=neon
)

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/platform
    ${CMAKE_SOURCE_DIR}/vita
    ${VITASDK}/arm-vita-eabi/include
    ${VITASDK}/arm-vita-eabi/include/vitaGL
)

# ── Sources ───────────────────────────────────────────────────────────────────
set(VITA_SOURCES
    # Vita entry point
    ${CMAKE_SOURCE_DIR}/vita/main_vita.c

    # Vita platform backends
    ${CMAKE_SOURCE_DIR}/platform/native_platform_vita.c
    ${CMAKE_SOURCE_DIR}/platform/native_renderer_vita.c
    ${CMAKE_SOURCE_DIR}/platform/native_libpad_vita.c

    # Upstream platform layer (audio via vitasdk SceAudio, CD via SceIo, etc.)
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
    ${CMAKE_SOURCE_DIR}/platform/native_renderer.c
    ${CMAKE_SOURCE_DIR}/platform/native_savestate.c
    ${CMAKE_SOURCE_DIR}/platform/native_state.c
    ${CMAKE_SOURCE_DIR}/platform/native_str.c

    # Game source (unchanged)
    ${CMAKE_SOURCE_DIR}/game/game_unity.c
    ${CMAKE_SOURCE_DIR}/game/zGlobal_RDATA.c
    ${CMAKE_SOURCE_DIR}/game/zGlobal_DATA.c
    ${CMAKE_SOURCE_DIR}/game/zGlobal_SDATA.c
)

add_executable(ctr_native_vita.elf ${VITA_SOURCES})

# ── Libraries ─────────────────────────────────────────────────────────────────
# vitaGL + its dependencies (order matters for static linking)
target_link_libraries(ctr_native_vita.elf
    vitaGL
    vitashark
    mathneon
    m
    SceDisplay_stub
    SceGxm_stub
    SceShaccCg_stub
    SceSysmodule_stub
    SceKernelDmacMgr_stub
    SceCtrl_stub
    SceAudio_stub
    ScePower_stub
    SceIofilemgr_stub
    SceRtc_stub
    SceLibKernel_stub
    SceNetCtl_stub
    SceNet_stub
    SceHttp_stub
    SceSsl_stub
    # Thread safety
    pthread
    # std
    c
)

# ── vita-mksfoex + vita-pack-vpk ─────────────────────────────────────────────
set(VITA_TITLEID  "CTRNATIV0")   # must be 9 chars: XXXXYYYYY0
set(VITA_VERSION  "01.00")

add_custom_command(
    OUTPUT eboot.bin
    COMMAND ${VITASDK}/bin/vita-elf-create
            $<TARGET_FILE:ctr_native_vita.elf>
            ctr_native_vita.velf
    COMMAND ${VITASDK}/bin/vita-make-fself -s
            ctr_native_vita.velf
            eboot.bin
    DEPENDS ctr_native_vita.elf
    COMMENT "Creating SELF from ELF"
)

add_custom_command(
    OUTPUT param.sfo
    COMMAND ${VITASDK}/bin/vita-mksfoex
            -s TITLE_ID=${VITA_TITLEID}
            -s APP_VER=${VITA_VERSION}
            "Crash Team Racing"
            param.sfo
    COMMENT "Generating param.sfo"
)

set(VPK_OUTPUT ${CMAKE_BINARY_DIR}/ctr_native.vpk)
add_custom_command(
    OUTPUT ${VPK_OUTPUT}
    COMMAND ${VITASDK}/bin/vita-pack-vpk
            -s param.sfo
            -b eboot.bin
            --add ${CMAKE_SOURCE_DIR}/vita/sce_sys/icon0.png=sce_sys/icon0.png
            --add ${CMAKE_SOURCE_DIR}/vita/sce_sys/livearea/contents/bg.png=sce_sys/livearea/contents/bg.png
            --add ${CMAKE_SOURCE_DIR}/vita/sce_sys/livearea/contents/startup.png=sce_sys/livearea/contents/startup.png
            --add ${CMAKE_SOURCE_DIR}/vita/sce_sys/livearea/contents/template.xml=sce_sys/livearea/contents/template.xml
            ${VPK_OUTPUT}
    DEPENDS eboot.bin param.sfo
    COMMENT "Packing VPK"
)

add_custom_target(vpk ALL DEPENDS ${VPK_OUTPUT})

install(FILES ${VPK_OUTPUT} DESTINATION .)
