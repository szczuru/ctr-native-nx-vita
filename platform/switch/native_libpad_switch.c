/*
 * native_libpad_switch.c
 * Replaces (or augments) native_libpad.c on Nintendo Switch.
 *
 * The game calls PadRead() / PadRead2() to get the PSX controller bitmask.
 * PSX uses active-LOW bitmask (0 = pressed).  We keep active-high internally
 * and invert here at the PSX boundary.
 *
 * Multitap / multiplayer:
 *   If > 1 Switch controller is connected, the game automatically receives
 *   multi-pad data, enabling 2–4 player split-screen as in the PS1 original
 *   (which also just detected additional pads on multitap, no link-cable needed
 *   for local play).
 *
 * Analogue sticks → PSX analogue pad emulation:
 *   The game's original code reads PadGetState() which includes analogue axes
 *   when the "DualShock" flag is set.  We supply fake analogue data so steering
 *   feels responsive.
 */

#ifdef PLATFORM_SWITCH

#include <stdint.h>
#include <string.h>

/* From native_platform_switch.c */
extern int      Platform_Switch_GetNumPlayers(void);
extern uint32_t Platform_Switch_GetButtons(int player);
extern int32_t  Platform_Switch_GetAxisLX(int player);
extern int32_t  Platform_Switch_GetAxisLY(int player);
extern int32_t  Platform_Switch_GetAxisRX(int player);
extern int32_t  Platform_Switch_GetAxisRY(int player);

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * PSX pad data structures (mirrors PsyQ library layout)
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
#define PAD_TYPE_DUALSHOCK  0x5   /* DualShock ID in PSX pad header */
#define PAD_LEN_DUALSHOCK   4     /* 4 halfwords of payload */

/* Standard PSX pad button word (each bit 0 = pressed, 1 = released) */
typedef uint16_t PsxButtons;

typedef struct __attribute__((packed))
{
    uint8_t  stat;       /* 0x00 = OK */
    uint8_t  len_type;   /* upper nibble: data length; lower nibble: type */
    uint16_t buttons;    /* active-low bitmask */
    uint8_t  right_x;   /* right stick X (128 = centre) */
    uint8_t  right_y;   /* right stick Y */
    uint8_t  left_x;    /* left stick X */
    uint8_t  left_y;    /* left stick Y */
} PsxPadData;

/*
 * Two pad slots per port (port 1 = pad 0/1, port 2 = pad 2/3 for multitap)
 * CTR uses 4 pad slots internally (PAD0A … PAD1B)
 */
#define MAX_PAD_SLOTS 4
static PsxPadData s_pad_data[MAX_PAD_SLOTS];

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * Axis normalisation: libnx uses -32767…32767; PSX uses 0…255 (centre=128)
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
static inline uint8_t AxisToByte(int32_t v)
{
    /* Clamp to [-32767, 32767] then scale to [0, 255] */
    if (v < -32767) v = -32767;
    if (v >  32767) v =  32767;
    return (uint8_t)(((v + 32767) * 255) / 65534);
}

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * Update all pad slots from Switch controller state
 * Called from the platform frame tick before the game's PadRead() calls.
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
void NativeLibPad_Switch_Update(void)
{
    int num = Platform_Switch_GetNumPlayers();

    for (int i = 0; i < MAX_PAD_SLOTS; i++)
    {
        PsxPadData *pd = &s_pad_data[i];

        if (i >= num)
        {
            /* No controller in this slot – report disconnected */
            pd->stat     = 0xFF;
            pd->len_type = 0x00;
            pd->buttons  = 0xFFFF;
            pd->right_x  = 128;
            pd->right_y  = 128;
            pd->left_x   = 128;
            pd->left_y   = 128;
            continue;
        }

        uint32_t btns = Platform_Switch_GetButtons(i);

        /* DualShock-style pad */
        pd->stat     = 0x00;
        pd->len_type = (uint8_t)((PAD_LEN_DUALSHOCK << 4) | PAD_TYPE_DUALSHOCK);

        /* Invert to active-low for PSX convention */
        pd->buttons  = (uint16_t)(~btns & 0xFFFF);

        /* Analogue sticks */
        pd->right_x = AxisToByte(Platform_Switch_GetAxisRX(i));
        pd->right_y = AxisToByte(-Platform_Switch_GetAxisRY(i)); /* Y-axis inverted */
        pd->left_x  = AxisToByte(Platform_Switch_GetAxisLX(i));
        pd->left_y  = AxisToByte(-Platform_Switch_GetAxisLY(i));
    }
}

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * PSX PadRead() equivalents
 * The game calls these (via native_libpad.c stubs).
 * We return a pointer to our prepared PsxPadData buffer.
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

/* Returns pad data for port 1 (players 1 & 2) */
const void *NativeLibPad_GetPort1Data(void)
{
    return &s_pad_data[0];   /* first player on port 1 */
}

/* Returns pad data for port 2 (players 3 & 4 via multitap equivalent) */
const void *NativeLibPad_GetPort2Data(void)
{
    return &s_pad_data[2];   /* first player on port 2 */
}

/* Returns how many pads are actually connected (1–4) */
int NativeLibPad_GetConnectedCount(void)
{
    return Platform_Switch_GetNumPlayers();
}

/* Raw button word for a specific player slot (0-based); returns 0xFFFF if absent */
uint16_t NativeLibPad_GetButtons(int slot)
{
    if (slot < 0 || slot >= MAX_PAD_SLOTS) return 0xFFFF;
    return s_pad_data[slot].buttons;
}

#endif /* PLATFORM_SWITCH */
