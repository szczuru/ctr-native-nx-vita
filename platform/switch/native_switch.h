#pragma once
/*
 * native_switch.h
 * Nintendo Switch homebrew platform header for CTR Native.
 * Replaces native_win32.h on Switch (devkitPro / libnx).
 *
 * Build with:
 *   PLATFORM=switch cmake --preset switch-arm64
 */

#ifdef __SWITCH__

#include <switch.h>
#include <stdint.h>
#include <stdio.h>

/* ── libnx console / applet boilerplate ──────────────────────────────────── */
static inline void NativeSwitch_AppletInit(void)
{
    /* Allow running as a regular NRO applet. */
    appletInitializeGamePlayRecording();
}

static inline void NativeSwitch_AppletExit(void)
{
    /* nothing extra needed */
}

/* ── Crash-safe stdout on Switch (redirected to nxlink or null) ───────────── */
#ifdef NXLINK_STDOUT
#include <unistd.h>
static inline void NativeSwitch_RedirectStdout(void)
{
    socketInitializeDefault();
    nxlinkStdio();
}
#else
static inline void NativeSwitch_RedirectStdout(void) { }
#endif

#endif /* __SWITCH__ */
