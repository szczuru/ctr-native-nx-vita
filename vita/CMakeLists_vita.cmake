# CMakeLists.txt dla PlayStation Vita (vitasdk + VitaGL)
# Plik docelowy: vita/CMakeLists.txt
#
# WAŻNE: cmake wywoływany jest z -S vita, więc CMAKE_SOURCE_DIR = vita/
# Katalog główny repozytorium przekazywany jest przez -DCTR_SOURCE_DIR=<root>
#
# Użycie:
#   cmake -B build-vita -S vita \
#         -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake \
#         -DCTR_SOURCE_DIR=$(pwd)

cmake_minimum_required(VERSION 3.20)
project(ctr_native_vita C)
set(CMAKE_C_STANDARD 17)

if(NOT DEFINED CTR_SOURCE_DIR)
    set(CTR_SOURCE_DIR "${CMAKE_SOURCE_DIR}/..")
endif()
get_filename_component(CTR_SOURCE_DIR "${CTR_SOURCE_DIR}" ABSOLUTE)

message(STATUS "CTR source root: ${CTR_SOURCE_DIR}")

# ── Flagi kompilacji ───────────────────────────────────────────────────────────
add_compile_options(
    -O2
    -ffunction-sections
    -fdata-sections
    -Wall
    -Wno-unused-function
    -Wno-strict-aliasing
    -mcpu=cortex-a9
    -mfpu=neon
    # Identyfikacja platformy
    -DPLATFORM_VITA
    -DCTR_NATIVE
    -DCTR_INTERNAL
)

# ── Include paths ──────────────────────────────────────────────────────────────
include_directories(
    ${CTR_SOURCE_DIR}
    ${CTR_SOURCE_DIR}/include
    ${CTR_SOURCE_DIR}/platform
    ${CMAKE_SOURCE_DIR}          # vita/ – nagłówki specyficzne dla platformy
    $ENV{VITASDK}/arm-vita-eabi/include
    $ENV{VITASDK}/arm-vita-eabi/include/vitaGL
)

# ── Źródła ────────────────────────────────────────────────────────────────────
set(VITA_SOURCES
    ${CMAKE_SOURCE_DIR}/main_vita.c

    ${CTR_SOURCE_DIR}/platform/native_platform_vita.c
    ${CTR_SOURCE_DIR}/platform/native_renderer_vita.c
    ${CTR_SOURCE_DIR}/platform/native_libpad_vita.c

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

    ${CTR_SOURCE_DIR}/game/game_unity.c
    ${CTR_SOURCE_DIR}/game/zGlobal_RDATA.c
    ${CTR_SOURCE_DIR}/game/zGlobal_DATA.c
    ${CTR_SOURCE_DIR}/game/zGlobal_SDATA.c
)

add_executable(ctr_native_vita.elf ${VITA_SOURCES})

target_compile_definitions(ctr_native_vita.elf PRIVATE
    CTR_NATIVE_VERSION=\"${CTR_NATIVE_VERSION}\"
)

# ── Biblioteki ─────────────────────────────────────────────────────────────────
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
    pthread
    c
)

# ── vita-make-fself + vita-pack-vpk ───────────────────────────────────────────
set(VITA_TITLEID "CTRNATIV0")
set(VITA_VERSION "01.00")

set(VITASDK $ENV{VITASDK})

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/ctr_native_vita.velf
    COMMAND ${VITASDK}/bin/vita-elf-create
            $<TARGET_FILE:ctr_native_vita.elf>
            ${CMAKE_BINARY_DIR}/ctr_native_vita.velf
    DEPENDS ctr_native_vita.elf
    COMMENT "Creating VELF"
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/eboot.bin
    COMMAND ${VITASDK}/bin/vita-make-fself -s
            ${CMAKE_BINARY_DIR}/ctr_native_vita.velf
            ${CMAKE_BINARY_DIR}/eboot.bin
    DEPENDS ${CMAKE_BINARY_DIR}/ctr_native_vita.velf
    COMMENT "Creating SELF"
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/param.sfo
    COMMAND ${VITASDK}/bin/vita-mksfoex
            -s TITLE_ID=${VITA_TITLEID}
            -s APP_VER=${VITA_VERSION}
            "Crash Team Racing"
            ${CMAKE_BINARY_DIR}/param.sfo
    COMMENT "Generating param.sfo"
)

set(VPK_OUTPUT ${CMAKE_BINARY_DIR}/ctr_native.vpk)

add_custom_command(
    OUTPUT ${VPK_OUTPUT}
    COMMAND ${VITASDK}/bin/vita-pack-vpk
            -s ${CMAKE_BINARY_DIR}/param.sfo
            -b ${CMAKE_BINARY_DIR}/eboot.bin
            --add ${CMAKE_SOURCE_DIR}/sce_sys/icon0.png=sce_sys/icon0.png
            --add ${CMAKE_SOURCE_DIR}/sce_sys/livearea/contents/bg.png=sce_sys/livearea/contents/bg.png
            --add ${CMAKE_SOURCE_DIR}/sce_sys/livearea/contents/startup.png=sce_sys/livearea/contents/startup.png
            --add ${CMAKE_SOURCE_DIR}/sce_sys/livearea/contents/template.xml=sce_sys/livearea/contents/template.xml
            ${VPK_OUTPUT}
    DEPENDS
        ${CMAKE_BINARY_DIR}/eboot.bin
        ${CMAKE_BINARY_DIR}/param.sfo
    COMMENT "Packing VPK"
)

add_custom_target(vpk ALL DEPENDS ${VPK_OUTPUT})
