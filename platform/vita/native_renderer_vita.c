/*
 * native_renderer_vita.c
 * VitaGL renderer adapter for CTR Native.
 *
 * VitaGL exposes a subset of OpenGL ES 2 / GL 1.x on the Vita GPU.
 * This file:
 *   1. Creates an offscreen texture that the PSX renderer draws into
 *   2. Blits it to the Vita screen with correct aspect-ratio viewport
 *   3. Applies optional bilinear / nearest filtering
 *   4. Implements 30/60 fps frame pacing via vblank
 *
 * Compile guard: PLATFORM_VITA
 */

#ifdef PLATFORM_VITA

#include <vitaGL.h>
#include <stdio.h>
#include <stdint.h>

extern int Platform_Vita_IsWidescreen(void);
extern int Platform_Vita_GetTargetFPS(void);
extern int Platform_Vita_IsBilinear(void);

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * Display constants
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
#define VITA_W     960
#define VITA_H     544
/* 4:3 pillarbox: 544 * 4/3 = 725.3 → 726, centred → x offset = (960-726)/2 = 117 */
#define PSX_W_43   726
#define PSX_X_43   117

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * Viewport selection
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
typedef struct { int x, y, w, h; } VitaRect;

static VitaRect VitaRenderer_CalcViewport(void)
{
    VitaRect r;
    if (Platform_Vita_IsWidescreen())
    {
        r.x = 0; r.y = 0; r.w = VITA_W; r.h = VITA_H;
    }
    else
    {
        r.x = PSX_X_43; r.y = 0; r.w = PSX_W_43; r.h = VITA_H;
    }
    return r;
}

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * Offscreen framebuffer
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
static GLuint s_fbo     = 0;
static GLuint s_fbo_tex = 0;

/* VitaGL supports FBOs via GL_EXT_framebuffer_object */
static void VitaRenderer_InitFBO(void)
{
    glGenTextures(1, &s_fbo_tex);
    glBindTexture(GL_TEXTURE_2D, s_fbo_tex);

    /* Vita framebuffer res: render at full 960×544 */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, VITA_W, VITA_H,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    GLenum filter = Platform_Vita_IsBilinear() ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffersEXT(1, &s_fbo);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, s_fbo);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                               GL_TEXTURE_2D, s_fbo_tex, 0);

    GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    if (status != GL_FRAMEBUFFER_COMPLETE_EXT)
        printf("[Vita GL] FBO incomplete: 0x%x\n", (unsigned)status);

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * Full-screen blit via GL immediate mode (VitaGL compatibility mode)
 * VitaGL supports GL 1.x / ES 1.1 style rendering without custom shaders.
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
static void VitaRenderer_BlitToScreen(void)
{
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    VitaRect vp = VitaRenderer_CalcViewport();
    glViewport(vp.x, vp.y, vp.w, vp.h);

    /* Ortho 2D projection for blit quad */
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

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * Frame pacing
 * VitaGL provides vblank sync via vglWaitVblankStart().
 * For 30 fps we wait 2 vblanks (60 Hz display / 2 = 30 Hz).
 * For 60 fps we wait 1 vblank.
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
static void VitaRenderer_FrameWait(void)
{
    int fps = Platform_Vita_GetTargetFPS();
    int vblanks = (fps >= 60) ? 1 : 2;
    for (int i = 0; i < vblanks; i++)
        vglWaitVblankStart(GL_TRUE);
}

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * Public API (called from main_vita.c / native_platform_vita.c)
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
void VitaRenderer_Init(void)
{
    VitaRenderer_InitFBO();
    printf("[Vita] Renderer init OK (bilinear=%d)\n",
           Platform_Vita_IsBilinear());
}

void VitaRenderer_Present(void)
{
    VitaRenderer_BlitToScreen();
    vglSwapBuffers(GL_FALSE);
    VitaRenderer_FrameWait();
}

GLuint VitaRenderer_GetFBO(void) { return s_fbo; }

#endif /* PLATFORM_VITA */
