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

#ifdef USE_DFB
#ifndef _DFBDEVICE_H
#define _DFBDEVICE_H
#include <stdint.h>

#include "DFBRect.h"
#include "../textrender/color.h"
#include <directfb.h>

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

const int SCREEN_WIDTH =  1280;
const int SCREEN_HEIGHT = 720;
const int SCREEN_BPP = 32;
static IDirectFB                 *dfb          = NULL;
static IDirectFBSurface          *screen       = NULL;
static IDirectFBFont             *font         = NULL;
static IDirectFBImageProvider    *image_provider = NULL;
static IDirectFBDataBuffer       *buffer       = NULL;
static IDirectFBSurface          *image        = NULL;
static DFBDataBufferDescription  ddsc;
static DFBSurfaceDescription     sdsc;

static const char       *filename = NULL;
static int screen_width, screen_height;

/* macro for a safe call to DirectFB functions */
#define DFBCHECK(x...) \
        {                                                                      \
           ret = x;                                                            \
           if (ret != DFB_OK) {                                                \
              fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ );           \
              DirectFBErrorFatal( #x, ret );                                   \
           }                                                                   \
        }

class DFBDevice {
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

    DFBDevice();
    ~DFBDevice();

    bool initCheck() { return mInited; }
    bool init();

    void clear(DFBRect *rect = nullptr) ;
    void clearSurface();
    void drawColor(float r, float g, float b, float a, bool flush = true);
    void drawColor(float r, float g, float b, float a, DFBRect& rect, bool flush = true);
    bool drawImage(int type, unsigned char *img, int64_t pts, int buffer_size, unsigned short spu_width, unsigned short spu_height, DFBRect &videoOriginRect, DFBRect &src, DFBRect &dst);
    DFBRect drawText(int type, TextParams& textParams, int64_t pts, int buffer_size, unsigned short spu_width, unsigned short spu_height, DFBRect &videoOriginRect, DFBRect &src, DFBRect &dst,
            int marginBottom = MIN_TEXT_MARGIN_BOTTOM, bool flush = true);
    void drawMultiText(int type, TextParams& textParams, int64_t pts, int buffer_size, unsigned short spu_width, unsigned short spu_height, DFBRect &videoOriginRect, DFBRect &src, DFBRect &dst);

    DFBRect screenRect() { return mScreenRect;}

private:
    bool createSubtitleOverlay();
    bool moveToBack();
    bool initDisplay();
    bool connectDisplay();
    void setupEnv(const char* runtimeDir, const char* waylandDisplay);
    bool createDisplay();
    bool initTexture(void* data, DFBRect &videoOriginRect, DFBRect &cropRect);
    void getScreenSize(size_t *width, size_t *height);
    void saveAsPNG(const char *filename, uint32_t *data, int width, int height);

    struct Texture {
        int width;
        int height;
        DFBRect crop;
    } mTexture;
private:
    DFBRect mScreenRect = DFBRect();
    DFBGLRect mScreenGLRect = DFBGLRect();

    bool isTextMultiPart = false;
    bool mInited = false;
};

#endif //_DFBDEVICE_H
#endif // USE_DFB
