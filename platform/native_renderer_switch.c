/*
 * native_renderer_switch.c
 * Nintendo Switch renderer adapter for CTR Native.
 *
 * The upstream renderer targets OpenGL 3.3 core (desktop).
 * libnx + SDL3 provide an EGL/OpenGL ES 2.0 context on Switch.
 *
 * This file:
 *   1. Patches GL calls that differ between GL 3.3 core and GLES 2
 *   2. Applies widescreen pillarbox / stretch viewport
 *   3. Applies optional bilinear / nearest-neighbour filtering
 *   4. Implements optional 60 fps frame pacing
 *
 * Compile guard: PLATFORM_SWITCH
 */

#ifdef PLATFORM_SWITCH

#include <GLES2/gl2.h>
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdint.h>

/* Config getters from native_platform_switch.c */
extern int Platform_Switch_IsWidescreen(void);
extern int Platform_Switch_GetTargetFPS(void);
extern int Platform_Switch_IsBilinear(void);
extern int Platform_Switch_IsIntegerScale(void);

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * Display constants
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
#define DISPLAY_W    1280
#define DISPLAY_H     720
#define PSX_RENDER_W  512   /* PSX framebuffer horizontal resolution */
#define PSX_RENDER_H  240   /* PSX framebuffer vertical resolution (NTSC) */
#define PSX_ASPECT    (4.0f / 3.0f)

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * Viewport calculation
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
typedef struct { int x, y, w, h; } Rect;

static Rect SwitchRenderer_CalcViewport(void)
{
    Rect vp;

    if (Platform_Switch_IsWidescreen())
    {
        /* Stretch to fill 1280×720 */
        vp.x = 0; vp.y = 0;
        vp.w = DISPLAY_W; vp.h = DISPLAY_H;
        return vp;
    }

    /* Maintain 4:3 with pillarboxes */
    if (Platform_Switch_IsIntegerScale())
    {
        /* Largest integer scale that fits 720p height while preserving 4:3 */
        /* PSX effective height after 240p→480i deinterlace = 480 */
        int scale = DISPLAY_H / 480;
        if (scale < 1) scale = 1;
        vp.h = 480 * scale;
        vp.w = (int)(vp.h * PSX_ASPECT);
    }
    else
    {
        /* Fit height */
        vp.h = DISPLAY_H;
        vp.w = (int)(DISPLAY_H * PSX_ASPECT);
    }

    vp.x = (DISPLAY_W - vp.w) / 2;
    vp.y = (DISPLAY_H - vp.h) / 2;
    return vp;
}

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * Blit shader (GLES 2 – trivial full-screen textured quad)
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
static const char *s_vert_src =
    "attribute vec2 aPos;\n"
    "attribute vec2 aUV;\n"
    "varying   vec2 vUV;\n"
    "void main() {\n"
    "  gl_Position = vec4(aPos, 0.0, 1.0);\n"
    "  vUV = aUV;\n"
    "}\n";

static const char *s_frag_src =
    "precision mediump float;\n"
    "varying vec2 vUV;\n"
    "uniform sampler2D uTex;\n"
    "void main() {\n"
    "  gl_FragColor = texture2D(uTex, vUV);\n"
    "}\n";

static GLuint s_prog      = 0;
static GLuint s_vbo       = 0;
static GLuint s_fbo_tex   = 0;   /* Offscreen render target (PSX framebuffer) */
static GLuint s_fbo       = 0;

static GLuint CompileShader(GLenum type, const char *src)
{
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    GLint ok; glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[512]; glGetShaderInfoLog(sh, sizeof(log), NULL, log);
        printf("[Switch GL] Shader error: %s\n", log);
    }
    return sh;
}

static void SwitchRenderer_InitShader(void)
{
    GLuint vert = CompileShader(GL_VERTEX_SHADER,   s_vert_src);
    GLuint frag = CompileShader(GL_FRAGMENT_SHADER, s_frag_src);
    s_prog = glCreateProgram();
    glAttachShader(s_prog, vert);
    glAttachShader(s_prog, frag);
    glBindAttribLocation(s_prog, 0, "aPos");
    glBindAttribLocation(s_prog, 1, "aUV");
    glLinkProgram(s_prog);
    glDeleteShader(vert);
    glDeleteShader(frag);

    /* Full-screen quad NDC coords + UV */
    float verts[] = {
        -1.0f, -1.0f,   0.0f, 1.0f,
         1.0f, -1.0f,   1.0f, 1.0f,
        -1.0f,  1.0f,   0.0f, 0.0f,
         1.0f,  1.0f,   1.0f, 0.0f,
    };
    glGenBuffers(1, &s_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
}

static void SwitchRenderer_InitFBO(void)
{
    /* Texture that receives the PSX framebuffer (upscaled to display res) */
    glGenTextures(1, &s_fbo_tex);
    glBindTexture(GL_TEXTURE_2D, s_fbo_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, DISPLAY_W, DISPLAY_H,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    int bilinear = Platform_Switch_IsBilinear();
    GLenum filter = bilinear ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    /* Framebuffer object */
    glGenFramebuffers(1, &s_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, s_fbo_tex, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        printf("[Switch GL] FBO incomplete: 0x%x\n", status);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SwitchRenderer_Init(void)
{
    SwitchRenderer_InitShader();
    SwitchRenderer_InitFBO();
    printf("[Switch] Renderer init OK (bilinear=%d)\n",
           Platform_Switch_IsBilinear());
}

/*
 * Called after the PSX framebuffer has been rendered into s_fbo.
 * Blits it to the screen with viewport / aspect-ratio correction.
 */
void SwitchRenderer_Present(SDL_Window *win)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    Rect vp = SwitchRenderer_CalcViewport();
    glViewport(vp.x, vp.y, vp.w, vp.h);

    glUseProgram(s_prog);
    glBindTexture(GL_TEXTURE_2D, s_fbo_tex);
    glUniform1i(glGetUniformLocation(s_prog, "uTex"), 0);

    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    SDL_GL_SwapWindow(win);
}

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * Frame pacing (30 or 60 fps)
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */
static uint64_t s_last_tick = 0;

void SwitchRenderer_FrameWait(void)
{
    int fps = Platform_Switch_GetTargetFPS();
    uint64_t frame_ns = (fps == 60)
        ? (uint64_t)(1000000000ULL / 60)
        : (uint64_t)(1000000000ULL / 30);

    uint64_t now = SDL_GetTicksNS();
    uint64_t elapsed = now - s_last_tick;
    if (elapsed < frame_ns)
        SDL_DelayNS(frame_ns - elapsed);
    s_last_tick = SDL_GetTicksNS();
}

/* FBO getter for upstream renderer to draw into */
GLuint SwitchRenderer_GetFBO(void) { return s_fbo; }

#endif /* PLATFORM_SWITCH */
