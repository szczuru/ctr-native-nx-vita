/*
 * platform/native_renderer_vita.c
 * VitaGL renderer adapter dla CTR Native.
 *
 * VitaGL implementuje core OpenGL ES 2 / GL 1.x.
 * Używamy funkcji BEZ sufiksu _EXT – VitaGL ma core FBO, nie rozszerzenie.
 */

#ifdef PLATFORM_VITA

#include <vitaGL.h>
#include <stdio.h>
#include <stdint.h>

extern int Platform_Vita_IsWidescreen(void);
extern int Platform_Vita_GetTargetFPS(void);
extern int Platform_Vita_IsBilinear(void);

/* ── Stałe wyświetlacza ──────────────────────────────────────────────────── */
#define VITA_W    960
#define VITA_H    544
#define PSX_W_43  726   /* 544 * 4/3 = 725.3 → 726 */
#define PSX_X_43  117   /* (960 - 726) / 2 */

typedef struct { int x, y, w, h; } VitaRect;

static VitaRect VitaRenderer_CalcViewport(void)
{
    VitaRect r;
    if (Platform_Vita_IsWidescreen()) {
        r.x = 0; r.y = 0; r.w = VITA_W; r.h = VITA_H;
    } else {
        r.x = PSX_X_43; r.y = 0; r.w = PSX_W_43; r.h = VITA_H;
    }
    return r;
}

/* ── Offscreen FBO ───────────────────────────────────────────────────────── */
static GLuint s_fbo     = 0;
static GLuint s_fbo_tex = 0;

static void VitaRenderer_InitFBO(void)
{
    glGenTextures(1, &s_fbo_tex);
    glBindTexture(GL_TEXTURE_2D, s_fbo_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, VITA_W, VITA_H,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    GLenum filter = Platform_Vita_IsBilinear() ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    /* Core FBO – VitaGL nie wymaga sufiksu _EXT */
    glGenFramebuffers(1, &s_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, s_fbo_tex, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        printf("[Vita GL] FBO incomplete: 0x%x\n", (unsigned)status);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/* ── Blit do ekranu (GL immediate mode – VitaGL compat layer) ───────────── */
static void VitaRenderer_BlitToScreen(void)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    VitaRect vp = VitaRenderer_CalcViewport();
    glViewport(vp.x, vp.y, vp.w, vp.h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, s_fbo_tex);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f, -1.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0f, -1.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f,  1.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0f,  1.0f);
    glEnd();

    glDisable(GL_TEXTURE_2D);
}

/* ── Frame pacing przez vblank ───────────────────────────────────────────── */
static void VitaRenderer_FrameWait(void)
{
    /* 60 Hz display: 1 vblank = 60fps, 2 vblanki = 30fps */
    int vblanks = (Platform_Vita_GetTargetFPS() >= 60) ? 1 : 2;
    for (int i = 0; i < vblanks; i++)
        vglWaitVblankStart(GL_TRUE);
}

/* ── Public API ──────────────────────────────────────────────────────────── */
void VitaRenderer_Init(void)
{
    VitaRenderer_InitFBO();
    printf("[Vita] Renderer init OK (bilinear=%d, fps=%d)\n",
           Platform_Vita_IsBilinear(), Platform_Vita_GetTargetFPS());
}

void VitaRenderer_Present(void)
{
    VitaRenderer_BlitToScreen();
    vglSwapBuffers(GL_FALSE);
    VitaRenderer_FrameWait();
}

GLuint VitaRenderer_GetFBO(void) { return s_fbo; }

#endif /* PLATFORM_VITA */
