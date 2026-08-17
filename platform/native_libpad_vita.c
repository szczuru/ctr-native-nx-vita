/*
 * native_libpad_vita.c
 * Replaces native_libpad.c on PlayStation Vita.
 *
 * Vita has one physical controller; no multitap support exists.
 * We present a single DualShock-compatible pad to the game engine.
 */

#ifdef PLATFORM_VITA

#include <stdint.h>
#include <string.h>

extern uint32_t Platform_Vita_GetButtons(void);
extern uint8_t  Platform_Vita_GetAxisLX(void);
extern uint8_t  Platform_Vita_GetAxisLY(void);
extern uint8_t  Platform_Vita_GetAxisRX(void);
extern uint8_t  Platform_Vita_GetAxisRY(void);

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * PSX pad layout (mirrors PsyQ)
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
#define PAD_TYPE_DUALSHOCK  0x5
#define PAD_LEN_DUALSHOCK   4

typedef struct __attribute__((packed))
{
    uint8_t  stat;
    uint8_t  len_type;
    uint16_t buttons;   /* active-low */
    uint8_t  right_x;
    uint8_t  right_y;
    uint8_t  left_x;
    uint8_t  left_y;
} PsxPadData;

static PsxPadData s_pad_data;

void NativeLibPad_Vita_Update(void)
{
    uint32_t btns = Platform_Vita_GetButtons();

    s_pad_data.stat     = 0x00;
    s_pad_data.len_type = (uint8_t)((PAD_LEN_DUALSHOCK << 4) | PAD_TYPE_DUALSHOCK);
    s_pad_data.buttons  = (uint16_t)(~btns & 0xFFFF);  /* active-low */

    /* Vita SceCtrl axes are already 0–255, centre=128 */
    s_pad_data.right_x = Platform_Vita_GetAxisRX();
    s_pad_data.right_y = Platform_Vita_GetAxisRY();
    s_pad_data.left_x  = Platform_Vita_GetAxisLX();
    s_pad_data.left_y  = Platform_Vita_GetAxisLY();
}

const void *NativeLibPad_GetPort1Data(void) { return &s_pad_data; }
const void *NativeLibPad_GetPort2Data(void) { return NULL; }   /* no second pad */
int         NativeLibPad_GetConnectedCount(void) { return 1; }
uint16_t    NativeLibPad_GetButtons(int slot)
{
    if (slot != 0) return 0xFFFF;
    return s_pad_data.buttons;
}

#endif /* PLATFORM_VITA */
