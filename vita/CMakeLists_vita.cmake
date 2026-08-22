# vita/CMakeLists.txt  (kopiowany z CMakeLists_vita.cmake przez workflow)
#
# Wywoływany z:
#   cmake -B build-vita -S vita
#         -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake
#         -DCTR_SOURCE_DIR=$(pwd)

cmake_minimum_required(VERSION 3.20)
project(ctr_native_vita C)
set(CMAKE_C_STANDARD 17)

if(NOT DEFINED CTR_SOURCE_DIR)
    set(CTR_SOURCE_DIR "${CMAKE_SOURCE_DIR}/..")
endif()
get_filename_component(CTR_SOURCE_DIR "${CTR_SOURCE_DIR}" ABSOLUTE)
message(STATUS "CTR source root: ${CTR_SOURCE_DIR}")

if(NOT DEFINED ENV{VITASDK})
    message(FATAL_ERROR "VITASDK environment variable is not set.")
endif()
set(VITASDK $ENV{VITASDK})

# ── Git hash ──────────────────────────────────────────────────────────────────
execute_process(
    COMMAND git rev-parse --short=12 HEAD
    WORKING_DIRECTORY ${CTR_SOURCE_DIR}
    OUTPUT_VARIABLE CTR_NATIVE_GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT CTR_NATIVE_GIT_HASH)
    set(CTR_NATIVE_GIT_HASH "unknown")
endif()
if(NOT CTR_NATIVE_VERSION)
    set(CTR_NATIVE_VERSION "dev")
endif()

# ── Include paths ──────────────────────────────────────────────────────────────
include_directories(
    ${CTR_SOURCE_DIR}
    ${CTR_SOURCE_DIR}/include
    ${CTR_SOURCE_DIR}/platform
    ${CTR_SOURCE_DIR}/game
    ${CMAKE_SOURCE_DIR}
    ${VITASDK}/arm-vita-eabi/include
    ${VITASDK}/arm-vita-eabi/include/vitaGL
)

# ── Źródła ────────────────────────────────────────────────────────────────────
add_executable(ctr_native_vita.elf
    ${CTR_SOURCE_DIR}/main.c
    ${CTR_SOURCE_DIR}/platform/native_platform_vita.c
    ${CTR_SOURCE_DIR}/platform/native_renderer_vita.c
    ${CTR_SOURCE_DIR}/platform/native_libpad_vita.c
)

# ── Flagi kompilacji PER TARGET ───────────────────────────────────────────────
# Używamy target_compile_options (nie add_compile_options) żeby mieć pewność
# że flagi trafiają do WSZYSTKICH jednostek translacji tego targetu, w tym main.c.
#
# -fno-short-enums: kluczowe – zapewnia sizeof(enum)==4 zgodnie z PSX ABI.
#   Bez tego CTR_STATIC_ASSERT w include/ovr_230.h failuje bo arm-vita-eabi-gcc
#   domyślnie używa krótkich enumów (sizeof(enum) może być 1 lub 2).
target_compile_options(ctr_native_vita.elf PRIVATE
    -mcpu=cortex-a9
    -mfpu=neon
    -fno-short-enums
    -O2
    -ffunction-sections
    -fdata-sections
    -Wall
    -Wno-unused-function
    -Wno-strict-aliasing
)

target_compile_definitions(ctr_native_vita.elf PRIVATE
    PLATFORM_VITA
    CTR_NATIVE
    CTR_INTERNAL
    CTR_NATIVE_VERSION=\"${CTR_NATIVE_VERSION}\"
    CTR_NATIVE_BUILD_ID=\"${CTR_NATIVE_GIT_HASH}\"
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

# ── Pakowanie VPK ─────────────────────────────────────────────────────────────
set(VITA_TITLEID "CTRNATIV0")
set(VITA_VERSION "01.00")

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
    DEPENDS ${CMAKE_BINARY_DIR}/eboot.bin ${CMAKE_BINARY_DIR}/param.sfo
    COMMENT "Packing VPK"
)

add_custom_target(vpk ALL DEPENDS ${VPK_OUTPUT})
