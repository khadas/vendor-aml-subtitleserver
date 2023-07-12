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
#ifdef USE_FB

#define LOG_TAG "FBDevice"
#include "FBDevice.h"
#include <Parser.h>

#include <cstdlib>
#include <utils/Log.h>
#include <cstring>
#include <vector>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <png.h>

#include "../textrender/text.h"
#include "../textrender/round_rectangle.h"
using namespace Cairo;

#define ENV_XDG_RUNTIME_DIR "XDG_RUNTIME_DIR"
#define ENV_WAYLAND_DISPLAY "WAYLAND_DISPLAY"
#define SUBTITLE_OVERLAY_NAME "subtitle-overlay"
#define FRAMEBUFFER_DEV "/dev/fb1"
//#define SUBTITLE_ZAPPER_4K

#ifdef ALOGD
#undef ALOGD
#endif
#define ALOGD(...) ALOGI(__VA_ARGS__)

class RdkShellCmd {
public:
    const int CMD_SIZE = 256;

    bool isDisplayExists(const char* displayDir, const char* displayName) {
        std::string path(displayDir);
        path += displayName;
        return access(path.c_str(), F_OK | R_OK) == 0;
    }


private:
    void executeCmd(const char* method, const char *cmd) {
        FILE* pFile = popen(cmd, "r");
        char buf[128];
        char* retStr = fgets(buf, sizeof(buf), pFile);
        ALOGD("[%s] ret= %s", method, retStr);
        pclose(pFile);
    }
};

static struct DisplayEnv {
    const char* xdg_runtime_dir;
    const char* wayland_display;
} sCandidateEnvs[] = {
        {"/run/user/0", "wayland-0"},
        {"/run", "wst-launcher"},
        {"/run", "wst-ResidentApp"},
        {"/run", "sub_overlay"},
        {"/run/HtmlApp", "wst-HtmlApp"},
};

FBDevice::FBDevice() {
    ALOGD("FBDevice +++");
    mInited = init();
}

FBDevice::~FBDevice() {
    /* release our interfaces to shutdown DirectFB */
    //font->Release( font );
    cleanupFramebuffer();
    ALOGD("FBDevice ---");
}

bool FBDevice::init() {
    ALOGD("FBDevice %s", __FUNCTION__);
    return initDisplay();
}

bool FBDevice::connectDisplay() {
    auto funcConnectFromPresetEnv = [&]()->bool {
        const char *runtimeDir = getenv(ENV_XDG_RUNTIME_DIR);
        const char *waylandDisplay = getenv(ENV_WAYLAND_DISPLAY);
        ALOGD("connectDisplay, current env= {%s, %s}", runtimeDir, waylandDisplay);
        if (runtimeDir != nullptr && strlen(runtimeDir) > 0
            && waylandDisplay != nullptr && strlen(waylandDisplay) > 0) {
            ALOGD("createDisplay with preset env");
            if (createDisplay()) {
                ALOGD("createDisplay success for {%s, %s}", runtimeDir, waylandDisplay);
                return true;
            } else {
                ALOGE("createDisplay failed for {%s, %s}", runtimeDir, waylandDisplay);
            }
        }

        return false;
    };

    auto funcConnectFromCandidatesEnvs = [&]()->bool {
        ALOGD("createDisplay with candidate envs");
        int n = sizeof(sCandidateEnvs) / sizeof(sCandidateEnvs[0]);
        for (int i = 0; i < n; ++i) {
            struct DisplayEnv& env = sCandidateEnvs[i];
            setupEnv(env.xdg_runtime_dir, env.wayland_display);

            if (createDisplay()) {
                ALOGD("createDisplay success for {%s, %s}", env.xdg_runtime_dir, env.wayland_display);
                return true;
            } else {
                ALOGE("createDisplay failed for {%s, %s}", env.xdg_runtime_dir, env.wayland_display);
            }
        }

        return false;
    };

    return funcConnectFromPresetEnv() || funcConnectFromCandidatesEnvs();
}

static void save2BitmapFile(const char *filename, uint32_t *bitmap, int w, int h) {
    FILE *f;
    char fname[40];
    snprintf(fname, sizeof(fname), "%s.ppm", filename);
    f = fopen(fname, "w");
    ALOGD("%s start", __FUNCTION__);
    if (!f) {
        ALOGE("Error cannot open file %s!", fname);
        return;
    }
    fprintf(f, "P6\n" "%d %d\n" "%d\n", w, h, 255);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int v = bitmap[y * w + x];
            putc((v >> 16) & 0xff, f);
            putc((v >> 8) & 0xff, f);
            putc((v >> 0) & 0xff, f);
        }
    }
    fclose(f);
}

bool FBDevice::initDisplay() {
    // init framebuffer
    if (!initFramebuffer()) {
        ALOGD( "Error: failed to initialize framebuffer\n");
        return false;
    }
    ALOGD("FBDevice  initDisplay start!");
    return true;
}

void FBDevice::setupEnv(const char* runtimeDir, const char* waylandDisplay) {
    ALOGD("setupEnv, ENV_XDG_RUNTIME_DIR= %s, ENV_WAYLAND_DISPLAY= %s",
            runtimeDir, waylandDisplay);

    setenv(ENV_XDG_RUNTIME_DIR, runtimeDir, 1);
    setenv(ENV_WAYLAND_DISPLAY, waylandDisplay, 1);
}

bool FBDevice::createDisplay() {
    ALOGD("dfb_display_roundtrip 222");
    return true;
}

void FBDevice::getScreenSize(size_t *width, size_t *height) {
    // From env WESTEROS_GL_GRAPHICS_MAX_SIZE
    const char *env = getenv("WESTEROS_GL_GRAPHICS_MAX_SIZE");
    if (env) {
        int w = 0, h = 0;
        if (sscanf(env, "%dx%d", &w, &h) == 2) {
            ALOGD("getScreenSize, from env WESTEROS_GL_GRAPHICS_MAX_SIZE: [%dx%d]", w, h);
            if ((w > 0) && (h > 0)) {
                *width = w;
                *height = h;
                return;
            }
        }
    }

    ALOGD("getScreenSize, from defult: [%dx%d]", WIDTH, HEIGHT);
    *width = WIDTH;
    *height = HEIGHT;
}

bool FBDevice::initTexture(void* data, FBRect &videoOriginRect, FBRect &cropRect) {
    ALOGD("sourceCrop glRect= [%d, %d, %d, %d]", cropRect.x(), cropRect.y(),
            cropRect.width(), cropRect.height());
    if (data == nullptr || cropRect.isEmpty())
        return false;

    if (!isTextMultiPart) {
    }
    //mTexture.crop.set(videoOriginRect);

    //Video original crop

    // Avoid data is out of display frame
    FBRect subCrop;
    if (!videoOriginRect.intersect(cropRect, &subCrop)) {
        ALOGE("Final subCrop is empty, return");
        return false;
    }
    subCrop.log("Final subCrop");
    return true;
}

void FBDevice::drawColor(float r, float g, float b, float a, bool flush) {
}

void FBDevice::drawColor(float r, float g, float b, float a, FBRect &rect, bool flush) {
    FBRect intersectRect = FBRect();
    if (rect.isEmpty() || !mScreenRect.intersect(rect, &intersectRect)) {
        ALOGW("%s, Rect checked failed", __FUNCTION__ );
        return;
    }
}

// init Framebuffer
bool FBDevice::initFramebuffer() {
    ALOGD("initFramebuffer ");
    // open Framebuffer dev
    mFbfd = open(FRAMEBUFFER_DEV, O_RDWR);
    if (mFbfd == -1) {
        ALOGD("Error: cannot open framebuffer device");
        return false;
    }

    // get screen information
    if (ioctl(mFbfd, FBIOGET_VSCREENINFO, &mVinfo) == -1) {
        ALOGD("Error: failed to get framebuffer information");
        close(mFbfd);
        return false;
    }

    // Set virtual screen size and virtual resolution
    mVinfo.bits_per_pixel = 32;
    mVinfo.xres_virtual = mVinfo.xres;
    mVinfo.yres_virtual = mVinfo.yres * 2;  // double buffering
    //set format is ARGB
    mVinfo.grayscale = 0;
    mVinfo.transp.offset = 24;
    mVinfo.transp.length = 8;
    mVinfo.transp.msb_right = 0;
    mVinfo.red.offset = 16;
    mVinfo.red.length = 8;
    mVinfo.red.msb_right = 0;
    mVinfo.green.offset = 8;
    mVinfo.green.length = 8;
    mVinfo.green.msb_right = 0;
    mVinfo.blue.offset = 0;
    mVinfo.blue.length = 8;
    mVinfo.blue.msb_right = 0;

    if (ioctl(mFbfd, FBIOPUT_VSCREENINFO, &mVinfo) == -1) {
        ALOGD("Error: failed to put framebuffer information");
        close(mFbfd);
        return false;
    }
    if (ioctl(mFbfd, FBIOGET_FSCREENINFO, &mFinfo) == -1) {
        ALOGD("Error: failed to put framebuffer information 2");
        close(mFbfd);
        return false;
    }

    // mapping framebuffer memory
    size_t fbSize = mVinfo.xres_virtual * mVinfo.yres_virtual * (mVinfo.bits_per_pixel / 8);
    mFramebuffer = (unsigned char*)mmap(NULL, fbSize, PROT_READ | PROT_WRITE, MAP_SHARED, mFbfd, 0);
    if (mFramebuffer == MAP_FAILED) {
        ALOGD("Error: failed to map framebuffer memory");
        close(mFbfd);
        return false;
    }
    print_screen_info(&mVinfo, &mFinfo);
    mCurSurface == 255;
    return true;
}

// clean Framebuffer
void FBDevice::cleanupFramebuffer() {
    if (mFramebuffer == NULL) {
        return;
    }
    // Unmap framebuffer memory
    if (munmap(mFramebuffer, mVinfo.xres_virtual * mVinfo.yres_virtual * (mVinfo.bits_per_pixel / 8)) == -1) {
        ALOGD("Error: failed to unmap framebuffer memory");
    }
    mFramebuffer = NULL;
    if (mFbfd != -1) {
        close(mFbfd);
        mFbfd = -1;
    }
}

void  FBDevice::print_screen_info(struct fb_var_screeninfo* varInfo, struct fb_fix_screeninfo* fixInfo )
{
    ALOGD("\n varinfo:res(%d, %d), virtual res(%d,%d), bpp(%d)\n"
            "grayscale(%d)\n (offset,len,msb_right) trans(%d,%d,%d) red(%d,%d,%d), green(%d,%d,%d), blue(%d,%d,%d)\n",
            varInfo->xres, varInfo->yres,varInfo->xres_virtual, varInfo->yres_virtual, varInfo->bits_per_pixel,
            varInfo->grayscale, varInfo->transp.offset, varInfo->transp.length, varInfo->transp.msb_right,
            varInfo->red.offset, varInfo->red.length, varInfo->red.msb_right,
            varInfo->green.offset, varInfo->green.length, varInfo->green.msb_right,
            varInfo->blue.offset, varInfo->blue.length,varInfo->blue.msb_right);
    ALOGD("\n fixInfo buffer:%lu buffer_len:%d,line_len:%d\n", fixInfo->smem_start, fixInfo->smem_len, fixInfo->line_length);
}

void FBDevice::clearFullFramebufferScreen() {
    size_t bufferSize = mVinfo.xres_virtual * mVinfo.yres_virtual * (mVinfo.bits_per_pixel / 8);
    memset(mFramebuffer, 0, bufferSize);
}

void FBDevice::clearFramebufferScreen(unsigned char* fbuffer, int buffer_len) {
    memset(fbuffer, 0, buffer_len);
}

//draw Image To Framebuffer
void FBDevice::drawImageToFramebuffer(unsigned char* fbuffer, unsigned char* imgBuffer, unsigned short spu_width, unsigned short spu_height,
                                FBRect& videoOriginRect, int type, float scale_factor) {
    ALOGD("%s start",__FUNCTION__);
    if (type == TYPE_SUBTITLE_DVB_TELETEXT) {
        // Calculate scaling factors
        #ifdef SUBTITLE_ZAPPER_4K
        float scale_factor_width = static_cast<float>(mVinfo.xres * 0.8) / spu_width;
        float scale_factor_height = static_cast<float>(mVinfo.yres * 0.8) / spu_height;
        #else
        float scale_factor_width = static_cast<float>(mVinfo.xres) / spu_width;
        float scale_factor_height = static_cast<float>(mVinfo.yres) / spu_height;
        #endif

        // Calculate scaled image dimensions
        int scaled_width = static_cast<int>(spu_width * scale_factor_width);
        int scaled_height = static_cast<int>(spu_height * scale_factor_height);

        ALOGD("drawImageToFramebuffer, scale_factor_width = %f, scale_factor_height = %f,scaled_width = %d,scaled_height = %d",
                                                scale_factor_width, scale_factor_height, scaled_width, scaled_height);
        // Calculate image offset
        int x_offset = (mVinfo.xres - scaled_width) / 2;
        int y_offset = (mVinfo.yres - scaled_height) / 2;

        // Copy and scale image data to framebuffer with position offset
        for (int y = 0; y < scaled_height; ++y) {
            for (int x = 0; x < scaled_width; ++x) {
                // Calculate corresponding coordinates in the original image
                int img_x = static_cast<int>(x / scale_factor_width);
                int img_y = static_cast<int>(y / scale_factor_height);

                // Calculate image and framebuffer offsets
                int imgOffset = (img_y * spu_width + img_x) * 4;  // 4 bytes per pixel
                int fbOffset = ((y + y_offset) * mVinfo.xres_virtual + (x + x_offset)) * 4;

                // Copy pixel data from the image buffer to the framebuffer
                memcpy(fbuffer + fbOffset, imgBuffer + imgOffset, 4);
            }
        }
    } else {
            int x_offset = 0;
            int y_offset = 0;
            x_offset = 0.15 * mVinfo.xres;
            y_offset = 0.8 * mVinfo.yres;

            int scaled_width = static_cast<int>(spu_width * scale_factor);
            int scaled_height = static_cast<int>(spu_height * scale_factor);

            // Copy and scale image data to framebuffer with position offset
            for (int y = 0; y < scaled_height; ++y) {
                for (int x = 0; x < scaled_width; ++x) {
                    int img_x = static_cast<int>(x / scale_factor);
                    int img_y = static_cast<int>(y / scale_factor);

                    int imgOffset = (img_y * spu_width + img_x) * 4;  // 4 bytes per pixel
                    int fbOffset = ((y + y_offset) * mVinfo.xres_virtual + (x + x_offset)) * 4;
                    memcpy(fbuffer + fbOffset, imgBuffer + imgOffset, 4);
                }
            }
    }

}

float setScaleFactor(FBRect& videoOriginRect) {
    float result = 1.0f;
    if (720.0f/videoOriginRect.width() < 1) {
        result = (720.0f/videoOriginRect.width())*1.5;
    }
    return result;
}

bool FBDevice::drawImage(int type, unsigned char* img, int64_t pts, int buffer_size,
                         unsigned short spu_width, unsigned short spu_height,
                         FBRect& videoOriginRect, FBRect& src, FBRect& dst) {
    ALOGD("%s start",__FUNCTION__);
    ALOGD("videoOriginRect.width() = %d, videoOriginRect.height() = %d, spu_width = %d,spu_height = %d",
                         videoOriginRect.width(), videoOriginRect.height(), spu_width, spu_height);

    int image_size  = (spu_width * spu_height * 4);  // 4 bytes per pixel
    float scale_factor = setScaleFactor(videoOriginRect);
    int buffer_len;
    buffer_len = mVinfo.xres * mVinfo.yres * 4;
    unsigned char* fbuffer = mFramebuffer;
    if (mCurSurface == 255)
    {
        mCurSur = 0;
        mVinfo.yoffset = 0;
    }
    if (mCurSurface == 0)
    {
        mCurSur = 1;
        mVinfo.yoffset = mVinfo.yres;
        fbuffer = fbuffer + buffer_len;
    }
    if (mCurSurface == 1)
    {
        mVinfo.yoffset = 0;
        mCurSur = 0;
    }
    clearFramebufferScreen(fbuffer,buffer_len);
    drawImageToFramebuffer(fbuffer, img, spu_width, spu_height, videoOriginRect, type, scale_factor);

    // refresh framebuffer
    if (ioctl(mFbfd, FBIOPAN_DISPLAY, &mVinfo) == -1) {
        ALOGD("Error: failed to pan display");
    }
    mCurSurface = mCurSur;
    return true;
}

void FBDevice::clear(FBRect * rect) {
    // if (rect == nullptr) {
    //     screen->Release( screen );
    //     dfb->Release( dfb );//CLear all screen
    // }
}

void FBDevice::clearSurface() {
    // if (screen == NULL) {
    //     return;
    // }
    // screen->Clear(screen, 0, 0, 0, 0);
    // screen->Flip(screen, NULL, DSFLIP_WAITFORSYNC);
}

static BoundingBox getFontBox(const char *content, FBRect &surfaceRect, Font& font) {
    Surface textSurface(surfaceRect.width(), surfaceRect.height());
    DrawContext drawContext(&textSurface);
    Text tmp(&drawContext, 0, 0, content, font);
    BoundingBox fontBox = tmp.calculateBounds(Pen(Colors::White));
    return fontBox;
}

void FBDevice::drawMultiText(int type, TextParams &textParams, int64_t pts, int buffer_size, unsigned short spu_width, unsigned short spu_height,
                                  FBRect &videoOriginRect, FBRect &src, FBRect &dst) {
    const char* content = textParams.content;
    auto funcSplitStr = [](std::string str, std::string pattern)->std::vector<std::string> {
        std::string::size_type pos;
        std::vector<std::string> result;
        str += pattern;
        size_t size = str.size();
        for (size_t i = 0; i < size; i++)
        {
            pos = str.find(pattern, i);
            if (pos < size)
            {
                std::string s = str.substr(i, pos - i);
                result.push_back(s);
                i = pos + pattern.size() - 1;
            }
        }
        return result;
    };

    std::vector<std::string> contents;

    if (strstr(content, "\n") != nullptr) {
        contents = funcSplitStr(content, "\n");
    } else if (strstr(content, "|") != nullptr) {
        contents = funcSplitStr(content, "|");
    } else {
        contents.emplace_back(content);
    }

    isTextMultiPart = false;
    int textPart = contents.size();
    ALOGD("[%s], contents, size= %d", __FUNCTION__, textPart);

    if (!contents.empty()) {
        int marginBottom = MIN_TEXT_MARGIN_BOTTOM;
        for (auto it = contents.rbegin(); it != contents.rend(); it++) {
            ALOGD("[%s], content= %s", __FUNCTION__, it->c_str());
            bool flush = (it + 1) == contents.rend();
            textParams.content = it->c_str();

            bool isNextEnd = (it + 2) == contents.rend();
            bool endIsEmpty = isNextEnd && ((strlen((it + 1)->c_str()) <= 0));
            if (endIsEmpty) {
                // For avoid first '\n' tag
                // We need flush on this part when end(next part) is empty
                flush = true;
            }

            FBRect textRect = drawText(type, textParams, pts, buffer_size, spu_width, spu_height, videoOriginRect, src, dst,
                    marginBottom, flush);

            if (endIsEmpty) {
                // We need finish when end is empty
                break;
            }

            isTextMultiPart =  textPart > 1;
            marginBottom += textRect.height();
        }
    }
}

FBRect FBDevice::drawText(int type, TextParams& textParams, int64_t pts, int buffer_size, unsigned short spu_width, unsigned short spu_height,
                                FBRect &videoOriginRect, FBRect &/*src*/, FBRect &dst, int marginBottom, bool flush) {

    const char* content = textParams.content;
    if (content == nullptr || strlen(content) <= 0) {
        ALOGE("Empty text, do not render");
        if (flush) clear();
        return FBRect::empty();
    }

    Font font;
    font.family = textParams.fontFamily;
    font.size = textParams.fontSize;

    BoundingBox fontBox = getFontBox(content, videoOriginRect, font);
    ALOGD("fontBox= [%f, %f, %f, %f]", fontBox.x, fontBox.y, fontBox.x2, fontBox.y2);
    if (fontBox.isEmpty()) {
        if (flush) clear();
        ALOGE_IF(strlen(content) > 0, "No support text type rendering");
        return FBRect::empty();
    }

    int padding = textParams.bgPadding;
    fontBox.move(padding, padding);
    fontBox.x -= padding;
    fontBox.y -= padding;
    fontBox.x2 += padding;
    fontBox.y2 += padding;

    Surface textSurface((int)fontBox.getWidth(), (int)fontBox.getHeight());
    DrawContext drawContext(&textSurface);

    Text text(&drawContext, 0, 0, content, font);

    if (textParams.usingBg) {
        // Background
        if (textParams.bgRoundRadius > 0) {
            RoundRectangle roundRect(&drawContext, fontBox.x, fontBox.y, fontBox.getWidth(),
                                     fontBox.getHeight(), textParams.bgRoundRadius);
            roundRect.draw(Colors::Transparent, textParams.bgColor);
        } else {
            Rectangle rectangle(&drawContext, fontBox.x, fontBox.y, fontBox.getWidth(), fontBox.getHeight());
            rectangle.draw(Colors::Transparent, textParams.bgColor);
        }
    }

    text.drawCenter(fontBox, textParams.textLineColor, textParams.textFillColor);

    //Move to center-bottom
    double moveX = (videoOriginRect.width() - fontBox.getWidth()) / 2;
    double moveY = videoOriginRect.height() - fontBox.getHeight() - marginBottom;
    fontBox.move(moveX, moveY);
    FBRect srcRect{(int)fontBox.x, (int)fontBox.y, (int)fontBox.x2, (int)fontBox.y2};

    unsigned char * data = textSurface.data();
    if (!data || !drawImage(type, data, pts, buffer_size, fontBox.getWidth(), fontBox.getHeight(), videoOriginRect, srcRect, dst)) {
        ALOGE("%s, No valid data will to be drew", __FUNCTION__);
        if (flush) clear();
        return FBRect::empty();
    }

    return srcRect;
}

// save png picture
void FBDevice::saveAsPNG(const char *filename, uint32_t *data, int width, int height) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        printf("Error: failed to open file %s\n", filename);
        return;
    }

    // init png_struct and png_info
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        printf("Error: failed to create png struct\n");
        fclose(fp);
        return;
    }
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        printf("Error: failed to create png info struct\n");
        png_destroy_write_struct(&png_ptr, NULL);
        fclose(fp);
        return;
    }

    // Set the error callback function of png_ptr
    if (setjmp(png_jmpbuf(png_ptr))) {
        printf("Error: failed to write png\n");
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        return;
    }

    // Initialize PNG I/O
    png_init_io(png_ptr, fp);

    // Write PNG file header
    png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);

    // write image data
    png_bytep row_pointers[height];
    for (int y = 0; y < height; y++) {
        row_pointers[y] = (png_bytep)&data[y * width];
    }
    png_write_image(png_ptr, row_pointers);

    // write end-of-file marker
    png_write_end(png_ptr, NULL);

    // clear memory
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
}


#endif // USE_FB
