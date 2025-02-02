/*
 * Copyright (C) 2014-2019 Amlogic, Inc. All rights reserved.
 *
 * All information contained herein is Amlogic confidential.
 *
 * This software is provided to you pursuant to Software License Agreement
 * (SLA) with Amlogic Inc ("Amlogic"). This software may be used
 * only in accordance with the terms of this agreement.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification is strictly prohibited without prior written permission from
 * Amlogic.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifdef USE_WAYLAND
#ifndef _WLGLDEVICE_H
#define _WLGLDEVICE_H

#include <stdint.h>

#define GL_GLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES

#include <GLES/gl.h>
#include <GLES/glext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "WLRect.h"
#include "../textrender/color.h"

#if __BYTE_ORDER == __LITTLE_ENDIAN
static const uint32_t kRed8     = 0x000000ff;
static const uint32_t kGreen8   = 0x0000ff00;
static const uint32_t kBlue8    = 0x00ff0000;

#define R32(argb)    static_cast<uint8_t>(argb & kRed8)
#define G32(argb)    static_cast<uint8_t>(((argb & kGreen8) >> 8) & 0xff)
#define B32(argb)    static_cast<uint8_t>(((argb & kBlue8) >> 16) & 0xff)
#define A32(argb)    static_cast<uint8_t>(((argb & kBlue8) >> 24) & 0xff)

#else   // __BYTE_ORDER
static const uint32_t kRed8     = 0x00ff0000;
static const uint32_t kGreen8   = 0x0000ff00;
static const uint32_t kBlue8    = 0x000000ff;

#define A32(argb)    static_cast<uint8_t>((argb & kBlue8) >> 24)
#define R32(argb)    static_cast<uint8_t>((argb & kRed8) >> 16)
#define G32(argb)    static_cast<uint8_t>((argb & kGreen8) >> 8)
#define B32(argb)    static_cast<uint8_t>(argb & kBlue8)
#endif  // __BYTE_ORDER

class WLGLDevice {
public:
    const static int WIDTH = 1920;
    const static int HEIGHT = 1080;
    const static int MIN_TEXT_MARGIN_BOTTOM = 50;

    typedef struct TextParams_ {
        const char* content;
        const char* fontFamily;
        int fontSize;

        bool usingBg = true;
        double bgRoundRadius;
        int bgPadding;

        Cairo::RGBA bgColor;
        Cairo::RGB textLineColor;
        Cairo::RGB textFillColor;
    } TextParams;

    WLGLDevice();
    ~WLGLDevice();

    bool initCheck() { return mInited; }
    bool init();

    void clear(WLGLRect *rect = nullptr) ;
    void drawColor(float r, float g, float b, float a, bool flush = true);
    void drawColor(float r, float g, float b, float a, WLRect& rect, bool flush = true);
    bool drawImage(void *img, WLRect &videoOriginRect, WLRect &src, WLRect &dst, bool flush = true);
    WLRect drawText(TextParams& textParams, WLRect &videoOriginRect, WLRect &src, WLRect &dst,
            int marginBottom = MIN_TEXT_MARGIN_BOTTOM, bool flush = true);
    void drawMultiText(TextParams& textParams, WLRect &videoOriginRect, WLRect &src, WLRect &dst);

    WLRect screenRect() { return mScreenRect;}

    struct wl_egl_window* wlEglWindow = nullptr;
private:
    bool createSubtitleOverlay();
    bool moveToBack();
    bool initDisplay();
    bool connectDisplay();
    bool initEGL();
    void setupEnv(const char* runtimeDir, const char* waylandDisplay);
    bool createDisplay();
    static void OnRegistry(void *data, struct wl_registry *registry, uint32_t name,
                                const char *interface, uint32_t version);
    bool initTexture(void* data, WLRect &videoOriginRect, WLRect &cropRect);
    void releaseEGL();
    void releaseWL();
    void glAssert(const char* where = nullptr);
    void getScreenSize(size_t *width, size_t *height);

    struct Texture {
        GLuint texture;
        int width;
        int height;
        WLRect crop;
    } mTexture;
private:
    struct wl_display *display = nullptr;
    struct wl_compositor *compositor = nullptr;
    struct wl_shell *shell = nullptr;
    struct wl_surface *surface = nullptr;
    struct wl_shell_surface *shellSurface = nullptr;

    EGLDisplay eglDisplay = nullptr;
    EGLContext eglContext = nullptr;
    EGLSurface eglSurface = nullptr;

    WLRect mScreenRect = WLRect();
    WLGLRect mScreenGLRect = WLGLRect();

    bool isTextMultiPart = false;
    bool mInited = false;
};

#endif //_WLGLDEVICE_H
#endif // USE_WAYLAND
