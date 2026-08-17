/*
 * native_platform_switch.c
 * Nintendo Switch homebrew platform backend for CTR Native.
 *
 * Compiled only when PLATFORM_SWITCH is defined (cmake -DPLATFORM=switch).
 *
 * Covers:
 *   - SDL3 window / GL init (OpenGL ES 2 via EGL provided by libnx)
 *   - Multi-controller input: Joy-Con pairs, Pro Controller, up to 4 players
 *   - Handheld split Joy-Con support
 *   - Audio via SDL3 audio subsystem (same as PC)
 *   - Memory card save files via libnx FS (sdmc:/ctr_native/)
 *   - CD / disc image via romfs (assets embedded in NRO) or sdmc:/ctr_native/assets/
 *   - Optional features: 16:9 widescreen, 60 fps target, enhanced filtering
 */

#ifdef PLATFORM_SWITCH

#include <switch.h>
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_switch.h"
#include "native_switch_config.h"   /* CTRSwitchConfig_Load / Save */

/* ── Forward declarations from the main platform layer ───────────────────── */
extern void Platform_Init(const char *title, int w, int h);
extern void Platform_Shutdown(void);

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * Switch-specific resolution / feature flags
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
#define SWITCH_DISPLAY_W   1280
#define SWITCH_DISPLAY_H    720
#define SWITCH_HANDHELD_W   1280
#define SWITCH_HANDHELD_H    720

/* PSX native resolution (4:3 pillarboxed inside 1280×720) */
#define PSX_NATIVE_W   800
#define PSX_NATIVE_H   600

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * Runtime config (loaded from sdmc:/ctr_native/config.ini)
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
typedef struct
{
    int widescreen;        /* 0 = 4:3 pillarbox, 1 = 16:9 stretched */
    int target_fps;        /* 30 or 60 */
    int bilinear_filter;   /* 0 = nearest (PSX authentic), 1 = bilinear */
    int integer_scale;     /* 0 = fill, 1 = integer scale only */
} SwitchRenderConfig;

static SwitchRenderConfig s_render_cfg = {
    .widescreen      = 0,
    .target_fps      = 30,
    .bilinear_filter = 0,
    .integer_scale   = 0,
};

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * Config file helpers
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
#define CFG_PATH "sdmc:/ctr_native/config.ini"

static void SwitchConfig_Load(void)
{
    FILE *f = fopen(CFG_PATH, "r");
    if (!f) return;

    char line[128];
    while (fgets(line, sizeof(line), f))
    {
        int val;
        if (sscanf(line, "widescreen=%d",    &val) == 1) s_render_cfg.widescreen      = !!val;
        if (sscanf(line, "target_fps=%d",    &val) == 1) s_render_cfg.target_fps      = (val == 60) ? 60 : 30;
        if (sscanf(line, "bilinear=%d",      &val) == 1) s_render_cfg.bilinear_filter = !!val;
        if (sscanf(line, "integer_scale=%d", &val) == 1) s_render_cfg.integer_scale   = !!val;
    }
    fclose(f);
}

static void SwitchConfig_Save(void)
{
    /* Ensure directory exists */
    mkdir("sdmc:/ctr_native", 0777);
    FILE *f = fopen(CFG_PATH, "w");
    if (!f) return;
    fprintf(f,
        "# CTR Native – Nintendo Switch config\n"
        "# widescreen: 0=4:3 pillarbox  1=16:9 stretch\n"
        "widescreen=%d\n"
        "# target_fps: 30 or 60\n"
        "target_fps=%d\n"
        "# bilinear: 0=nearest (PSX-authentic)  1=bilinear\n"
        "bilinear=%d\n"
        "# integer_scale: 0=fill  1=integer steps only\n"
        "integer_scale=%d\n",
        s_render_cfg.widescreen,
        s_render_cfg.target_fps,
        s_render_cfg.bilinear_filter,
        s_render_cfg.integer_scale
    );
    fclose(f);
}

/* Expose to the rest of the engine */
int  Platform_Switch_IsWidescreen(void)    { return s_render_cfg.widescreen; }
int  Platform_Switch_GetTargetFPS(void)    { return s_render_cfg.target_fps; }
int  Platform_Switch_IsBilinear(void)      { return s_render_cfg.bilinear_filter; }
int  Platform_Switch_IsIntegerScale(void)  { return s_render_cfg.integer_scale; }

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * Multi-controller / multiplayer input
 *
 * The PS1 multitap allowed up to 4 controllers.
 * On Switch we map:
 *   Player 1  – first connected Joy-Con pair or Pro Controller
 *   Player 2  – second pair / Pro Controller (split Joy-Con in handheld: both halves)
 *   Player 3  – third controller
 *   Player 4  – fourth controller
 *
 * The game already supports local multiplayer if more than one pad is seen.
 * PadState from libnx is polled every frame and translated to the PSX button
 * bitmask the game expects.
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
#define MAX_PLAYERS 4

/* PSX pad button bitmask (active-low in retail; we keep active-high here
 * and invert at the interface boundary in native_libpad.c) */
#define PSX_BTN_SELECT   (1 << 0)
#define PSX_BTN_L3       (1 << 1)
#define PSX_BTN_R3       (1 << 2)
#define PSX_BTN_START    (1 << 3)
#define PSX_BTN_UP       (1 << 4)
#define PSX_BTN_RIGHT    (1 << 5)
#define PSX_BTN_DOWN     (1 << 6)
#define PSX_BTN_LEFT     (1 << 7)
#define PSX_BTN_L2       (1 << 8)
#define PSX_BTN_R2       (1 << 9)
#define PSX_BTN_L1       (1 << 10)
#define PSX_BTN_R1       (1 << 11)
#define PSX_BTN_TRIANGLE (1 << 12)
#define PSX_BTN_CIRCLE   (1 << 13)
#define PSX_BTN_CROSS    (1 << 14)
#define PSX_BTN_SQUARE   (1 << 15)

typedef struct
{
    PadState  pad;
    int       active;      /* 1 if a controller is present for this slot */
    uint32_t  psx_buttons; /* translated PSX bitmask, last polled frame   */
    int32_t   axis_lx;     /* analogue left stick X  (-32768..32767)       */
    int32_t   axis_ly;     /* analogue left stick Y  (-32768..32767)       */
    int32_t   axis_rx;     /* analogue right stick X                       */
    int32_t   axis_ry;     /* analogue right stick Y                       */
} SwitchPlayer;

static SwitchPlayer s_players[MAX_PLAYERS];
static int          s_num_players = 0;

static void SwitchInput_Init(void)
{
    padConfigureInput(MAX_PLAYERS, HidNpadStyleSetFullCtrl);

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        padInitialize(&s_players[i].pad, (HidNpadIdType)i);
        s_players[i].active      = 0;
        s_players[i].psx_buttons = 0;
    }

    /* Also initialise single-player handheld (Joy-Con pair as one) */
    padInitialize(&s_players[0].pad, HidNpadIdType_Handheld);
}

static uint32_t SwitchInput_TranslateButtons(uint64_t held)
{
    uint32_t psx = 0;

    /* D-pad */
    if (held & HidNpadButton_Up)    psx |= PSX_BTN_UP;
    if (held & HidNpadButton_Down)  psx |= PSX_BTN_DOWN;
    if (held & HidNpadButton_Left)  psx |= PSX_BTN_LEFT;
    if (held & HidNpadButton_Right) psx |= PSX_BTN_RIGHT;

    /* Face buttons (A/B/X/Y → Circle/Cross/Triangle/Square on PSX) */
    if (held & HidNpadButton_A)  psx |= PSX_BTN_CIRCLE;
    if (held & HidNpadButton_B)  psx |= PSX_BTN_CROSS;
    if (held & HidNpadButton_X)  psx |= PSX_BTN_TRIANGLE;
    if (held & HidNpadButton_Y)  psx |= PSX_BTN_SQUARE;

    /* Shoulders */
    if (held & HidNpadButton_L)   psx |= PSX_BTN_L1;
    if (held & HidNpadButton_R)   psx |= PSX_BTN_R1;
    if (held & HidNpadButton_ZL)  psx |= PSX_BTN_L2;
    if (held & HidNpadButton_ZR)  psx |= PSX_BTN_R2;

    /* Sticks */
    if (held & HidNpadButton_StickL)  psx |= PSX_BTN_L3;
    if (held & HidNpadButton_StickR)  psx |= PSX_BTN_R3;

    /* System */
    if (held & HidNpadButton_Plus)   psx |= PSX_BTN_START;
    if (held & HidNpadButton_Minus)  psx |= PSX_BTN_SELECT;

    return psx;
}

/*
 * Called once per frame from the native platform layer.
 * Fills s_players[] and updates s_num_players.
 */
void SwitchInput_Poll(void)
{
    s_num_players = 0;

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        padUpdate(&s_players[i].pad);

        /* A controller is "active" if it has a connected style */
        HidNpadStyleTag style = padGetStyleSet(&s_players[i].pad);
        s_players[i].active = (style != HidNpadStyleTag_NpadStyleNone);

        if (s_players[i].active)
        {
            s_num_players = i + 1;

            uint64_t held = padGetButtons(&s_players[i].pad);
            s_players[i].psx_buttons = SwitchInput_TranslateButtons(held);

            HidAnalogStickState ls = padGetStickPos(&s_players[i].pad, 0);
            HidAnalogStickState rs = padGetStickPos(&s_players[i].pad, 1);
            s_players[i].axis_lx = ls.x;
            s_players[i].axis_ly = ls.y;
            s_players[i].axis_rx = rs.x;
            s_players[i].axis_ry = rs.y;
        }
        else
        {
            s_players[i].psx_buttons = 0;
            s_players[i].axis_lx = 0;
            s_players[i].axis_ly = 0;
            s_players[i].axis_rx = 0;
            s_players[i].axis_ry = 0;
        }
    }
}

/* Public getters used by native_libpad.c */
int      Platform_Switch_GetNumPlayers(void)              { return s_num_players; }
uint32_t Platform_Switch_GetButtons(int player)
{
    if (player < 0 || player >= MAX_PLAYERS) return 0;
    return s_players[player].psx_buttons;
}
int32_t Platform_Switch_GetAxisLX(int player)
{
    if (player < 0 || player >= MAX_PLAYERS) return 0;
    return s_players[player].axis_lx;
}
int32_t Platform_Switch_GetAxisLY(int player)
{
    if (player < 0 || player >= MAX_PLAYERS) return 0;
    return s_players[player].axis_ly;
}
int32_t Platform_Switch_GetAxisRX(int player)
{
    if (player < 0 || player >= MAX_PLAYERS) return 0;
    return s_players[player].axis_rx;
}
int32_t Platform_Switch_GetAxisRY(int player)
{
    if (player < 0 || player >= MAX_PLAYERS) return 0;
    return s_players[player].axis_ry;
}

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * Memory card – save files on sdmc:/ctr_native/
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
#define MEMCARD_DIR  "sdmc:/ctr_native"
#define MEMCARD_A    MEMCARD_DIR "/carda.mcr"
#define MEMCARD_B    MEMCARD_DIR "/cardb.mcr"
#define MEMCARD_SIZE (128 * 1024)   /* 128 KB – standard PS1 memory card */

static void SwitchFS_EnsureDir(void)
{
    mkdir(MEMCARD_DIR, 0777);
}

/* Returns file size or 0 on error */
static size_t SwitchFS_ReadFile(const char *path, void *buf, size_t max)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t n = fread(buf, 1, max, f);
    fclose(f);
    return n;
}

static int SwitchFS_WriteFile(const char *path, const void *buf, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    size_t n = fwrite(buf, 1, len, f);
    fclose(f);
    return (n == len) ? 0 : -1;
}

/*
 * Public memcard API – mirrors the PC native_memcard.c interface.
 * The platform layer calls these; the game never touches the FS directly.
 */
int Platform_Switch_MemcardRead(int slot, void *buf, size_t size)
{
    SwitchFS_EnsureDir();
    const char *path = (slot == 0) ? MEMCARD_A : MEMCARD_B;
    size_t n = SwitchFS_ReadFile(path, buf, size);
    if (n == 0)
    {
        /* No save yet – return blank card (0xFF = erased flash) */
        memset(buf, 0xFF, size);
    }
    return 0;
}

int Platform_Switch_MemcardWrite(int slot, const void *buf, size_t size)
{
    SwitchFS_EnsureDir();
    const char *path = (slot == 0) ? MEMCARD_A : MEMCARD_B;
    return SwitchFS_WriteFile(path, buf, size);
}

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * Asset / CD path resolution
 *
 * Priority:
 *   1. romfs:/  (embedded in NRO – assets baked at build time)
 *   2. sdmc:/ctr_native/assets/  (user-provided disc image / extracted files)
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
#define ROMFS_ASSET_DIR  "romfs:/assets"
#define SDMC_ASSET_DIR   "sdmc:/ctr_native/assets"

static int s_romfs_mounted = 0;

static void SwitchAssets_Init(void)
{
    Result rc = romfsInit();
    if (R_SUCCEEDED(rc))
    {
        s_romfs_mounted = 1;
        printf("[Switch] romfs mounted\n");
    }
    else
    {
        printf("[Switch] romfs not available – using sdmc assets\n");
    }
}

static void SwitchAssets_Exit(void)
{
    if (s_romfs_mounted)
        romfsExit();
}

/*
 * Returns the base asset directory path.
 * The PC layer calls NativeAssets_GetAssetDir(); we redirect that here.
 */
const char *Platform_Switch_GetAssetDir(void)
{
    if (s_romfs_mounted)
        return ROMFS_ASSET_DIR;
    return SDMC_ASSET_DIR;
}

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * Platform Init / Shutdown
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
void Platform_Switch_Init(void)
{
    NativeSwitch_AppletInit();
    NativeSwitch_RedirectStdout();

    SwitchConfig_Load();
    SwitchAssets_Init();
    SwitchInput_Init();

    int w = s_render_cfg.widescreen ? SWITCH_DISPLAY_W : PSX_NATIVE_W;
    int h = s_render_cfg.widescreen ? SWITCH_DISPLAY_H : PSX_NATIVE_H;
    Platform_Init("Crash Team Racing", w, h);

    printf("[Switch] widescreen=%d  target_fps=%d  bilinear=%d  integer_scale=%d\n",
        s_render_cfg.widescreen,
        s_render_cfg.target_fps,
        s_render_cfg.bilinear_filter,
        s_render_cfg.integer_scale);
}

void Platform_Switch_Shutdown(void)
{
    SwitchConfig_Save();
    SwitchAssets_Exit();
    NativeSwitch_AppletExit();
    Platform_Shutdown();
}

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * Per-frame hook (called from main loop)
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
void Platform_Switch_FrameTick(void)
{
    SwitchInput_Poll();

    /* HOME button exit */
    if (appletGetOperationMode() == AppletOperationMode_Handheld)
    {
        /* nothing extra */
    }
}

#endif /* PLATFORM_SWITCH */
