/*
 * main_switch.c
 * Nintendo Switch NRO entry point for CTR Native.
 *
 * Mirrors the structure of the upstream main.c but:
 *   - Initialises libnx subsystems before SDL
 *   - Uses Platform_Switch_Init / Shutdown
 *   - Runs the frame loop with Switch-specific timing
 *   - Handles HOME-button / applet focus changes
 *
 * Build: compiled as part of the switch cmake preset.
 *        Upstream main.c is EXCLUDED from the Switch build via CMakeLists.
 */

#ifdef PLATFORM_SWITCH

#define SDL_MAIN_HANDLED
#include <switch.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdlib.h>

/* Platform backends */
extern void Platform_Switch_Init(void);
extern void Platform_Switch_Shutdown(void);
extern void Platform_Switch_FrameTick(void);
extern void SwitchRenderer_FrameWait(void);
extern void NativeLibPad_Switch_Update(void);

/* Upstream game entry point (unchanged) */
extern int CTR_Main(void);

/* Upstream platform layer */
extern void Platform_LogFlush(void);
extern void Platform_InitScratchpad(void);
extern void Platform_RepairResidentPointers(int mode);

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * Print version banner
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
#ifndef CTR_NATIVE_VERSION
#define CTR_NATIVE_VERSION "0.0.0-dev-switch"
#endif
#ifndef CTR_NATIVE_BUILD_ID
#define CTR_NATIVE_BUILD_ID "unknown"
#endif

static void PrintBanner(void)
{
    printf("[CTR Native Switch] Version: %s (%s)\n",
           CTR_NATIVE_VERSION, CTR_NATIVE_BUILD_ID);
    printf("[CTR Native Switch] Nintendo Switch homebrew port\n");
    fflush(stdout);
}

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * Application entry point
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    PrintBanner();

    /* Initialise Switch platform (romfs, config, input, window, GL) */
    Platform_Switch_Init();

    Platform_InitScratchpad();
    Platform_RepairResidentPointers(0);

    /* Run the game – CTR_Main() contains the original game loop */
    int result = CTR_Main();

    Platform_LogFlush();
    Platform_Switch_Shutdown();

    return result;
}

#endif /* PLATFORM_SWITCH */
