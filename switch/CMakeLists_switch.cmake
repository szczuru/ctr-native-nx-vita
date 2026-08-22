# switch/CMakeLists.txt  (kopiowany z CMakeLists_switch.cmake przez workflow)
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

# ── SDL3 ze źródeł vendorowanych ──────────────────────────────────────────────
# Upstream ctr-native vendoruje SDL3 w externals/SDL/.
# Budujemy go dla Switcha z wyłączonymi backendami których brak na bare-metal.
# Wymagane: NIE używamy SDL_THREADS (brak POSIX threads na Switch),
# używamy SDL_VIDEO_OPENGL_ES2 przez EGL z libnx.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)
set(SDL_TEST   OFF CACHE BOOL "" FORCE)

# Wyłączamy subsystemy niedostępne na Switch
set(SDL_AUDIO        ON  CACHE BOOL "" FORCE)   # libnx audio
set(SDL_VIDEO        ON  CACHE BOOL "" FORCE)
set(SDL_RENDER       OFF CACHE BOOL "" FORCE)
set(SDL_JOYSTICK     OFF CACHE BOOL "" FORCE)   # używamy libnx pad bezpośrednio
set(SDL_HAPTIC       OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR       OFF CACHE BOOL "" FORCE)
set(SDL_POWER        OFF CACHE BOOL "" FORCE)
set(SDL_FILESYSTEM   OFF CACHE BOOL "" FORCE)
set(SDL_THREADS      OFF CACHE BOOL "" FORCE)   # KLUCZOWE: brak POSIX threads w libnx
set(SDL_TIMERS       ON  CACHE BOOL "" FORCE)
set(SDL_LOADSO       OFF CACHE BOOL "" FORCE)
set(SDL_LOCALE       OFF CACHE BOOL "" FORCE)
set(SDL_MISC         OFF CACHE BOOL "" FORCE)
set(SDL_OPENGL       OFF CACHE BOOL "" FORCE)
set(SDL_OPENGLES     ON  CACHE BOOL "" FORCE)   # GLES2 przez EGL

add_subdirectory(${CTR_SOURCE_DIR}/externals/SDL
                 ${CMAKE_BINARY_DIR}/externals/SDL
                 EXCLUDE_FROM_ALL)

# ── Include paths ──────────────────────────────────────────────────────────────
include_directories(
    ${CTR_SOURCE_DIR}
    ${CTR_SOURCE_DIR}/include
    ${CTR_SOURCE_DIR}/platform
    ${CTR_SOURCE_DIR}/game
    ${CMAKE_SOURCE_DIR}
    ${DEVKITPRO}/libnx/include
    ${DEVKITPRO}/portlibs/switch/include
    # SDL3 nagłówki z vendored source
    ${CTR_SOURCE_DIR}/externals/SDL/include
)

# ── Źródła ────────────────────────────────────────────────────────────────────
add_executable(ctr_native_switch.elf
    ${CTR_SOURCE_DIR}/main.c
    ${CTR_SOURCE_DIR}/platform/native_platform_switch.c
    ${CTR_SOURCE_DIR}/platform/native_renderer_switch.c
    ${CTR_SOURCE_DIR}/platform/native_libpad_switch.c
)

# ── Flagi kompilacji ───────────────────────────────────────────────────────────
target_compile_options(ctr_native_switch.elf PRIVATE
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
)

target_compile_definitions(ctr_native_switch.elf PRIVATE
    PLATFORM_SWITCH
    __SWITCH__
    CTR_NATIVE
    CTR_INTERNAL
    CTR_NATIVE_SKIP_BITWIDTH_CHECK=1
    CTR_NATIVE_VERSION=\"${CTR_NATIVE_VERSION}\"
    CTR_NATIVE_BUILD_ID=\"${CTR_NATIVE_GIT_HASH}\"
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
    SDL3-static
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

if(EXISTS ${ICONFILE})
    set(ICON_ARG --icon=${ICONFILE})
else()
    set(ICON_ARG "")
    message(STATUS "switch/icon.jpg not found – NRO will use default icon")
endif()

add_custom_command(
    OUTPUT ${NRO_OUTPUT}
    COMMAND ${DEVKITPRO}/tools/bin/elf2nro
            $<TARGET_FILE:ctr_native_switch.elf>
            ${NRO_OUTPUT}
            --nacp=${NACPFILE}
            ${ICON_ARG}
    DEPENDS ctr_native_switch.elf ${NACPFILE}
    COMMENT "Generating NRO"
)

add_custom_target(nro ALL DEPENDS ${NRO_OUTPUT})
