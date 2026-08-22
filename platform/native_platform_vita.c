/*
 * platform/native_platform_vita.c
 * PlayStation Vita homebrew platform backend dla CTR Native.
 */

#ifdef PLATFORM_VITA

#include <vitasdk.h>
#include <vitaGL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Stałe ───────────────────────────────────────────────────────────────── */
#define VITA_DISPLAY_W  960
#define VITA_DISPLAY_H  544
#define DATA_DIR        "ux0:/data/ctr_native"
#define CFG_PATH        DATA_DIR "/config.ini"
#define MEMCARD_A       DATA_DIR "/carda.mcr"
#define MEMCARD_B       DATA_DIR "/cardb.mcr"

/* ── Config ──────────────────────────────────────────────────────────────── */
typedef struct {
    int widescreen;
    int target_fps;
    int bilinear_filter;
} VitaRenderConfig;

static VitaRenderConfig s_render_cfg = { 0, 30, 0 };

static void VitaConfig_Load(void)
{
    SceUID f = sceIoOpen(CFG_PATH, SCE_O_RDONLY, 0);
    if (f < 0) return;
    char buf[512];
    int n = sceIoRead(f, buf, sizeof(buf) - 1);
    sceIoClose(f);
    if (n <= 0) return;
    buf[n] = '\0';
    char *line = strtok(buf, "\n");
    while (line) {
        int val;
        if (sscanf(line, "widescreen=%d", &val) == 1)  s_render_cfg.widescreen      = !!val;
        if (sscanf(line, "target_fps=%d", &val) == 1)  s_render_cfg.target_fps      = (val == 60) ? 60 : 30;
        if (sscanf(line, "bilinear=%d",   &val) == 1)  s_render_cfg.bilinear_filter = !!val;
        line = strtok(NULL, "\n");
    }
}

static void VitaConfig_Save(void)
{
    sceIoMkdir(DATA_DIR, 0777);
    SceUID f = sceIoOpen(CFG_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    if (f < 0) return;
    char buf[512];
    int n = snprintf(buf, sizeof(buf),
        "# CTR Native PS Vita config\n"
        "widescreen=%d\ntarget_fps=%d\nbilinear=%d\n",
        s_render_cfg.widescreen, s_render_cfg.target_fps, s_render_cfg.bilinear_filter);
    sceIoWrite(f, buf, n);
    sceIoClose(f);
}

int Platform_Vita_IsWidescreen(void)  { return s_render_cfg.widescreen; }
int Platform_Vita_GetTargetFPS(void)  { return s_render_cfg.target_fps; }
int Platform_Vita_IsBilinear(void)    { return s_render_cfg.bilinear_filter; }

/* ── Input ───────────────────────────────────────────────────────────────── */
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

typedef struct {
    uint32_t psx_buttons;
    uint8_t  axis_lx, axis_ly, axis_rx, axis_ry;
} VitaPadState;

static VitaPadState s_pad;

static void VitaInput_Init(void)
{
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
}

static uint32_t VitaInput_TranslateButtons(unsigned int held)
{
    uint32_t psx = 0;
    if (held & SCE_CTRL_UP)       psx |= PSX_BTN_UP;
    if (held & SCE_CTRL_DOWN)     psx |= PSX_BTN_DOWN;
    if (held & SCE_CTRL_LEFT)     psx |= PSX_BTN_LEFT;
    if (held & SCE_CTRL_RIGHT)    psx |= PSX_BTN_RIGHT;
    if (held & SCE_CTRL_CROSS)    psx |= PSX_BTN_CROSS;
    if (held & SCE_CTRL_CIRCLE)   psx |= PSX_BTN_CIRCLE;
    if (held & SCE_CTRL_SQUARE)   psx |= PSX_BTN_SQUARE;
    if (held & SCE_CTRL_TRIANGLE) psx |= PSX_BTN_TRIANGLE;
    if (held & SCE_CTRL_L1)       psx |= PSX_BTN_L1;
    if (held & SCE_CTRL_R1)       psx |= PSX_BTN_R1;
    if (held & SCE_CTRL_L2)       psx |= PSX_BTN_L2;
    if (held & SCE_CTRL_R2)       psx |= PSX_BTN_R2;
    if (held & SCE_CTRL_L3)       psx |= PSX_BTN_L3;
    if (held & SCE_CTRL_R3)       psx |= PSX_BTN_R3;
    if (held & SCE_CTRL_START)    psx |= PSX_BTN_START;
    if (held & SCE_CTRL_SELECT)   psx |= PSX_BTN_SELECT;
    return psx;
}

void VitaInput_Poll(void)
{
    SceCtrlData ctrl;
    sceCtrlReadBufferPositive(0, &ctrl, 1);
    s_pad.psx_buttons = VitaInput_TranslateButtons(ctrl.buttons);
    s_pad.axis_lx = ctrl.lx;
    s_pad.axis_ly = ctrl.ly;
    s_pad.axis_rx = ctrl.rx;
    s_pad.axis_ry = ctrl.ry;
}

uint32_t Platform_Vita_GetButtons(void) { return s_pad.psx_buttons; }
uint8_t  Platform_Vita_GetAxisLX(void)  { return s_pad.axis_lx; }
uint8_t  Platform_Vita_GetAxisLY(void)  { return s_pad.axis_ly; }
uint8_t  Platform_Vita_GetAxisRX(void)  { return s_pad.axis_rx; }
uint8_t  Platform_Vita_GetAxisRY(void)  { return s_pad.axis_ry; }

/* ── Memory card ─────────────────────────────────────────────────────────── */
int Platform_Vita_MemcardRead(int slot, void *buf, size_t size)
{
    const char *path = (slot == 0) ? MEMCARD_A : MEMCARD_B;
    SceUID f = sceIoOpen(path, SCE_O_RDONLY, 0);
    if (f < 0) { memset(buf, 0xFF, size); return 0; }
    sceIoRead(f, buf, size);
    sceIoClose(f);
    return 0;
}

int Platform_Vita_MemcardWrite(int slot, const void *buf, size_t size)
{
    sceIoMkdir(DATA_DIR, 0777);
    const char *path = (slot == 0) ? MEMCARD_A : MEMCARD_B;
    SceUID f = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    if (f < 0) return -1;
    sceIoWrite(f, buf, size);
    sceIoClose(f);
    return 0;
}

/* ── Assets ──────────────────────────────────────────────────────────────── */
static const char *s_asset_dir = NULL;

static void VitaAssets_Init(void)
{
    SceUID dh = sceIoDopen("app0:/assets");
    if (dh >= 0) {
        sceIoDclose(dh);
        s_asset_dir = "app0:/assets";
    } else {
        sceIoMkdir(DATA_DIR "/assets", 0777);
        s_asset_dir = DATA_DIR "/assets";
    }
    printf("[Vita] Assets: %s\n", s_asset_dir);
}

const char *Platform_Vita_GetAssetDir(void)
{
    return s_asset_dir ? s_asset_dir : DATA_DIR "/assets";
}

/* ── VitaGL init ─────────────────────────────────────────────────────────── */
static void VitaGL_Init(void)
{
    vglInit(0x400000);  /* 4 MB VRAM */
    vglWaitVblankStart(GL_TRUE);
}

/* ── Platform Init / Shutdown ────────────────────────────────────────────── */
void Platform_Vita_Init(void)
{
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);

    VitaConfig_Load();
    VitaAssets_Init();
    VitaInput_Init();
    VitaGL_Init();

    printf("[Vita] widescreen=%d  target_fps=%d  bilinear=%d\n",
        s_render_cfg.widescreen, s_render_cfg.target_fps, s_render_cfg.bilinear_filter);
}

void Platform_Vita_Shutdown(void)
{
    VitaConfig_Save();
    /* Zamknięcie VitaGL */
    vglEnd();
}

void Platform_Vita_FrameTick(void)
{
    VitaInput_Poll();
}

#endif /* PLATFORM_VITA */
