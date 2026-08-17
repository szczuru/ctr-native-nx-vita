/*
 * main_vita.c
 * PlayStation Vita VPK entry point for CTR Native.
 *
 * Replaces upstream main.c in the Vita build.
 * Uses VitaGL directly (no SDL3 on Vita).
 */

#ifdef PLATFORM_VITA

#include <vitasdk.h>
#include <vitaGL.h>
#include <stdio.h>
#include <stdlib.h>

extern void Platform_Vita_Init(void);
extern void Platform_Vita_Shutdown(void);
extern void Platform_Vita_FrameTick(void);
extern void VitaRenderer_Init(void);
extern void VitaRenderer_Present(void);
extern void NativeLibPad_Vita_Update(void);

/* Upstream game entry point */
extern int CTR_Main(void);

/* Upstream platform initialisation hooks */
extern void Platform_InitScratchpad(void);
extern void Platform_RepairResidentPointers(int mode);
extern void Platform_LogFlush(void);

#ifndef CTR_NATIVE_VERSION
#define CTR_NATIVE_VERSION "0.0.0-dev-vita"
#endif
#ifndef CTR_NATIVE_BUILD_ID
#define CTR_NATIVE_BUILD_ID "unknown"
#endif

/* VPK module requirements */
int _newlib_heap_size_user = 192 * 1024 * 1024;  /* 192 MB heap */

int main(void)
{
    printf("[CTR Native Vita] Version: %s (%s)\n",
           CTR_NATIVE_VERSION, CTR_NATIVE_BUILD_ID);
    fflush(stdout);

    Platform_Vita_Init();
    VitaRenderer_Init();

    Platform_InitScratchpad();
    Platform_RepairResidentPointers(0);

    int result = CTR_Main();

    Platform_LogFlush();
    Platform_Vita_Shutdown();

    return result;
}

#endif /* PLATFORM_VITA */
