#pragma once
/*
 * platform_ports.h
 * Shared header included by platform.h to expose port-specific
 * feature flags and dispatch functions to the rest of the codebase.
 *
 * Include order:  platform.h  →  platform_ports.h  (this file)
 *
 * Only one PLATFORM_* block is active per build.
 */

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * Nintendo Switch
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
#ifdef PLATFORM_SWITCH

/* Init / shutdown (called from main_switch.c) */
void Platform_Switch_Init(void);
void Platform_Switch_Shutdown(void);
void Platform_Switch_FrameTick(void);

/* Renderer */
void     SwitchRenderer_Init(void);
void     SwitchRenderer_Present(struct SDL_Window *win);
void     SwitchRenderer_FrameWait(void);
unsigned SwitchRenderer_GetFBO(void);   /* GLuint */

/* Input */
void     SwitchInput_Poll(void);
int      Platform_Switch_GetNumPlayers(void);
unsigned Platform_Switch_GetButtons(int player);   /* uint32_t */
int      Platform_Switch_GetAxisLX(int player);
int      Platform_Switch_GetAxisLY(int player);
int      Platform_Switch_GetAxisRX(int player);
int      Platform_Switch_GetAxisRY(int player);

/* Pad */
void           NativeLibPad_Switch_Update(void);
const void    *NativeLibPad_GetPort1Data(void);
const void    *NativeLibPad_GetPort2Data(void);
int            NativeLibPad_GetConnectedCount(void);
unsigned short NativeLibPad_GetButtons(int slot);  /* uint16_t */

/* Memory card */
int Platform_Switch_MemcardRead (int slot, void *buf,       unsigned size);
int Platform_Switch_MemcardWrite(int slot, const void *buf, unsigned size);

/* Assets */
const char *Platform_Switch_GetAssetDir(void);

/* Config queries */
int Platform_Switch_IsWidescreen(void);
int Platform_Switch_GetTargetFPS(void);
int Platform_Switch_IsBilinear(void);
int Platform_Switch_IsIntegerScale(void);

/* Map generic platform calls to Switch implementations */
#define Platform_GetAssetDir()          Platform_Switch_GetAssetDir()
#define Platform_MemcardRead(s,b,n)     Platform_Switch_MemcardRead(s,b,n)
#define Platform_MemcardWrite(s,b,n)    Platform_Switch_MemcardWrite(s,b,n)
#define Platform_FrameBegin()           Platform_Switch_FrameTick(); NativeLibPad_Switch_Update()
#define Platform_FrameEnd(w)            SwitchRenderer_Present(w); SwitchRenderer_FrameWait()

#endif /* PLATFORM_SWITCH */

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * PlayStation Vita
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
#ifdef PLATFORM_VITA

/* Init / shutdown */
void Platform_Vita_Init(void);
void Platform_Vita_Shutdown(void);
void Platform_Vita_FrameTick(void);

/* Renderer */
void     VitaRenderer_Init(void);
void     VitaRenderer_Present(void);
unsigned VitaRenderer_GetFBO(void);   /* GLuint */

/* Input */
void           VitaInput_Poll(void);
unsigned       Platform_Vita_GetButtons(void);   /* uint32_t */
unsigned char  Platform_Vita_GetAxisLX(void);
unsigned char  Platform_Vita_GetAxisLY(void);
unsigned char  Platform_Vita_GetAxisRX(void);
unsigned char  Platform_Vita_GetAxisRY(void);

/* Pad */
void           NativeLibPad_Vita_Update(void);
const void    *NativeLibPad_GetPort1Data(void);
const void    *NativeLibPad_GetPort2Data(void);
int            NativeLibPad_GetConnectedCount(void);
unsigned short NativeLibPad_GetButtons(int slot);

/* Memory card */
int Platform_Vita_MemcardRead (int slot, void *buf,       unsigned size);
int Platform_Vita_MemcardWrite(int slot, const void *buf, unsigned size);

/* Assets */
const char *Platform_Vita_GetAssetDir(void);

/* Config queries */
int Platform_Vita_IsWidescreen(void);
int Platform_Vita_GetTargetFPS(void);
int Platform_Vita_IsBilinear(void);

/* Map generic platform calls to Vita implementations */
#define Platform_GetAssetDir()          Platform_Vita_GetAssetDir()
#define Platform_MemcardRead(s,b,n)     Platform_Vita_MemcardRead(s,b,n)
#define Platform_MemcardWrite(s,b,n)    Platform_Vita_MemcardWrite(s,b,n)
#define Platform_FrameBegin()           Platform_Vita_FrameTick(); NativeLibPad_Vita_Update()
#define Platform_FrameEnd(w)            VitaRenderer_Present()

#endif /* PLATFORM_VITA */
