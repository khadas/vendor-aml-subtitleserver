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

#define LOG_TAG "DFBDevice"
#include "DFBDevice.h"
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
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

#include <pthread.h>

#include <directfb.h>
#include <png.h>

#include "../textrender/text.h"
#include "../textrender/round_rectangle.h"
using namespace Cairo;

#define ENV_XDG_RUNTIME_DIR "XDG_RUNTIME_DIR"
#define ENV_WAYLAND_DISPLAY "WAYLAND_DISPLAY"
#define SUBTITLE_OVERLAY_NAME "subtitle-overlay"

#ifdef ALOGD
#undef ALOGD
#endif
#define ALOGD(...) ALOGI(__VA_ARGS__)

class RdkShellCmd {
public:
    const int CMD_SIZE = 256;

    bool createDisplay(const char* client, const char* displayName) {
        char cmdStr[CMD_SIZE];
        sprintf(cmdStr, mCmdCreateDisplay.c_str(), client, displayName);
        executeCmd(__FUNCTION__, cmdStr);
        return isDisplayExists("/run/", displayName);
    }

    bool isDisplayExists(const char* displayDir, const char* displayName) {
        std::string path(displayDir);
        path += displayName;
        return access(path.c_str(), F_OK | R_OK) == 0;
    }

    void moveToBack(const char* displayName) {
        char cmdStr[CMD_SIZE];
        sprintf(cmdStr, mCmdMoveToBack.c_str(), displayName);
        executeCmd(__FUNCTION__, cmdStr);
    }

private:
    void executeCmd(const char* method, const char *cmd) {
        FILE* pFile = popen(cmd, "r");
        char buf[128];
        char* retStr = fgets(buf, sizeof(buf), pFile);
        ALOGD("[%s] ret= %s", method, retStr);
        pclose(pFile);
    }

private:
    std::string mCmdCreateDisplay = R"(curl 'http://127.0.0.1:9998/jsonrpc' -d '{"jsonrpc": "2.0","id": 4,"method":
    "org.rdk.RDKShell.1.createDisplay","params": { "client": "%s", "displayName": "%s" }}';)";

    std::string mCmdMoveToBack = R"(curl 'http://127.0.0.1:9998/jsonrpc' -d '{"jsonrpc": "2.0","id": 4,"method":
    "org.rdk.RDKShell.1.moveToBack", "params": { "client": "%s" }}';)";
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

DFBDevice::DFBDevice() {
    ALOGD("DFBDevice +++");
    mInited = init();
}

DFBDevice::~DFBDevice() {
    /* release our interfaces to shutdown DirectFB */
    //font->Release( font );
    ALOGD("DFBDevice ---");
}

bool DFBDevice::init() {
    ALOGD("DFBDevice %s", __FUNCTION__);
    return initDisplay();
}

bool DFBDevice::connectDisplay() {
    auto funcConnectFromSubOverlay = [&]()->bool {
        if (createSubtitleOverlay()) {
            setupEnv("/run", SUBTITLE_OVERLAY_NAME);
            ALOGD("createDisplay from shell cmd");
            if (createDisplay()) {
                ALOGV("createDisplay success for {%s, %s}", "/run", SUBTITLE_OVERLAY_NAME);
                return true;
            } else {
                ALOGE("createDisplay failed for {%s, %s}", "/run", SUBTITLE_OVERLAY_NAME);
            }
        }

        return false;
    };

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

    return funcConnectFromSubOverlay() || funcConnectFromPresetEnv() || funcConnectFromCandidatesEnvs();
}

static IDirectFBSurface *load_image(std::string filename)
{
    DFBResult ret;
    IDirectFBSurface *surf;
    DFBSurfaceDescription dsc;
    ret = dfb->CreateImageProvider (dfb,  filename.c_str(), &image_provider);
    if (ret != DFB_OK) {
        ALOGD("CreateImageProvider Fail!");
        return NULL;
    }

    image_provider->GetSurfaceDescription(image_provider, &dsc);
    dfb->CreateSurface(dfb, &dsc, &surf);
    image_provider->RenderTo(image_provider, surf, NULL);
    image_provider->Release(image_provider);

    return surf;
}

static void apply_surface(int x, int y, IDirectFBSurface *source, IDirectFBSurface *destination, int type, unsigned short spu_width , unsigned short spu_height)
{
    destination->Clear ( destination, 255, 255, 255, 0 );
    destination->SetBlittingFlags(destination, DSBLIT_SRC_COLORKEY);
    source->SetSrcColorKey(source, 0xFF, 0x0, 0xFF);
    destination->DisableAcceleration(destination, DFXL_BLIT);
    if (type ==TYPE_SUBTITLE_DVB_TELETEXT) {
        destination->StretchBlit(screen, image, NULL, NULL);
    } else {
        destinationRect.x= x;
        destinationRect.y= y;
        destinationRect.w=spu_width * 0.6;
        destinationRect.h=spu_height * 0.6;
        destination->StretchBlit(destination, image, NULL, &destinationRect);
    }
}

static void save2BitmapFile(const char *filename, uint32_t *bitmap, int w, int h) {
    FILE *f;
    char fname[40];
    snprintf(fname, sizeof(fname), "%s.png", filename);
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

bool DFBDevice::createSubtitleOverlay() {
    RdkShellCmd rdkShellCmd;

    if (rdkShellCmd.isDisplayExists("/run/", SUBTITLE_OVERLAY_NAME)) {
        ALOGD("createSubtitleOverlay, %s has already exist", "/run/" SUBTITLE_OVERLAY_NAME);
        return true;
    }

    if (rdkShellCmd.createDisplay(SUBTITLE_OVERLAY_NAME, SUBTITLE_OVERLAY_NAME)) {
        ALOGD("createSubtitleOverlay OK");
        rdkShellCmd.moveToBack(SUBTITLE_OVERLAY_NAME);
        return true;
    }

    ALOGE("Create Subtitle overlay failed");

    return false;
}

bool DFBDevice::initDisplay() {
    ALOGD("DFBDevice  initDisplay start!");
    DFBResult ret;
    /* disable mouse icon,init directfb command line parsing.*/
    int argx = 2;
    char *argData[] = {"self", "--dfb:system=fbdev,fbdev=/dev/fb1,mode=640x360,depth=8,pixelformat=ARGB,no-hardware", 0};
    char **argPointer = argData;
    ret = DirectFBInit(&argx, &argPointer);
    if (ret != DFB_OK) {
        ALOGD("DirectFBInit DirectFB Fail!");
        return  false;
    }
    ret = DirectFBCreate(&dfb);
    if (ret != DFB_OK) {
        ALOGD("DirectFBCreate Fail!");
        return  false;
    }

    ret = dfb->SetCooperativeLevel (dfb, DFSCL_NORMAL);
    if (ret != DFB_OK) {
        ALOGD("SetCooperativeLevel Fail!");
        return  false;
    }

    //default fullscreen
    /*ret = dfb->SetVideoMode(dfb, SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_BPP);
    if (ret != DFB_OK) {
        ALOGD("SetVideoMode Fail!");
        return  false;
    }*/

    sdsc.flags = DSDESC_CAPS;
    sdsc.caps = (DFBSurfaceCapabilities)(DSCAPS_PRIMARY | DSCAPS_FLIPPING);
    ret = dfb->CreateSurface(dfb, &sdsc, &screen);
    if (ret != DFB_OK) {
        ALOGD("CreateSurface Fail!");
        return  false;
    }
    //screen->SetBlittingFlags( screen, DSBLIT_BLEND_ALPHACHANNEL );
    screen->Clear ( screen, 255, 255, 255, 0 );

    screen->GetSize (screen, &screen_width, &screen_height);
    ALOGD("DFBDevice screen_width = %d,screen_height = %d", screen_width, screen_height);
    ALOGD("DFBDevice  initDisplay end!");
    return true;
}

void DFBDevice::setupEnv(const char* runtimeDir, const char* waylandDisplay) {
    ALOGD("setupEnv, ENV_XDG_RUNTIME_DIR= %s, ENV_WAYLAND_DISPLAY= %s",
            runtimeDir, waylandDisplay);

    setenv(ENV_XDG_RUNTIME_DIR, runtimeDir, 1);
    setenv(ENV_WAYLAND_DISPLAY, waylandDisplay, 1);
}

bool DFBDevice::createDisplay() {
    ALOGD("dfb_display_roundtrip 222");
    return true;
}

void DFBDevice::getScreenSize(size_t *width, size_t *height) {
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

bool DFBDevice::initTexture(void* data, DFBRect &videoOriginRect, DFBRect &cropRect) {
    ALOGD("sourceCrop glRect= [%d, %d, %d, %d]", cropRect.x(), cropRect.y(),
            cropRect.width(), cropRect.height());
    if (data == nullptr || cropRect.isEmpty())
        return false;

    if (!isTextMultiPart) {
    }
    //mTexture.crop.set(videoOriginRect);

    //Video original crop

    // Avoid data is out of display frame
    DFBRect subCrop;
    if (!videoOriginRect.intersect(cropRect, &subCrop)) {
        ALOGE("Final subCrop is empty, return");
        return false;
    }
    subCrop.log("Final subCrop");
    return true;
}

void DFBDevice::drawColor(float r, float g, float b, float a, bool flush) {
}

void DFBDevice::drawColor(float r, float g, float b, float a, DFBRect &rect, bool flush) {
    DFBRect intersectRect = DFBRect();
    if (rect.isEmpty() || !mScreenRect.intersect(rect, &intersectRect)) {
        ALOGW("%s, Rect checked failed", __FUNCTION__ );
        return;
    }
}

bool DFBDevice::drawImage(int type, unsigned char *img, int64_t pts, int buffer_size, unsigned short spu_width , unsigned short spu_height, DFBRect &videoOriginRect, DFBRect &src, DFBRect &dst) {
    if (!initTexture(img, videoOriginRect, src)) {
        ALOGE("%s: initTexture failed", __FUNCTION__ );
        return false;
    }
    char *filename;
    ALOGD("DFBDevice %s img:%p buffer_size:%d pts:%lld, spu_width:%d spu_height:%d src.x():%d ,src.y():%d ,src.width():%d ,src.height():%d start", __FUNCTION__, img, buffer_size, pts, spu_width, spu_height, src.x(),src.y(),src.width(),src.height());
    filename = "/tmp/subtitle_dfb";
    saveAsPNG(filename, (uint32_t *)img, spu_width, spu_height);
//    save2BitmapFile(filename, (uint32_t *)img, spu_width, spu_height);
    DFBResult             ret;
    image = load_image(std::string(filename));
    apply_surface(screen_width*0.2,screen_height*0.8  , image, screen, type, spu_width, spu_height);
    //apply_surface( 240, 190, image, screen );
    screen->Flip (screen, NULL, DSFLIP_WAITFORSYNC);
    //screen->StretchBlit(screen, image, NULL, NULL);
    image->Release(image);
    ALOGD("DFBDevice %s end", __FUNCTION__);
    return true;
}

void DFBDevice::clear(DFBRect * rect) {
    if (rect == nullptr) {
        screen->Release( screen );
        dfb->Release( dfb );//CLear all screen
    }
}

void DFBDevice::clearSurface() {
    if (screen == NULL) {
        return;
    }
    screen->Clear(screen, 0, 0, 0, 0);
    screen->Flip(screen, NULL, DSFLIP_WAITFORSYNC);
}

static BoundingBox getFontBox(const char *content, DFBRect &surfaceRect, Font& font) {
    Surface textSurface(surfaceRect.width(), surfaceRect.height());
    DrawContext drawContext(&textSurface);
    Text tmp(&drawContext, 0, 0, content, font);
    BoundingBox fontBox = tmp.calculateBounds(Pen(Colors::White));
    return fontBox;
}

void DFBDevice::drawMultiText(int type, TextParams &textParams, int64_t pts, int buffer_size, unsigned short spu_width, unsigned short spu_height,
                                  DFBRect &videoOriginRect, DFBRect &src, DFBRect &dst) {
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

            DFBRect textRect = drawText(type, textParams, pts, buffer_size, spu_width, spu_height, videoOriginRect, src, dst,
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

DFBRect DFBDevice::drawText(int type, TextParams& textParams, int64_t pts, int buffer_size, unsigned short spu_width, unsigned short spu_height,
                                DFBRect &videoOriginRect, DFBRect &/*src*/, DFBRect &dst, int marginBottom, bool flush) {

    const char* content = textParams.content;
    if (content == nullptr || strlen(content) <= 0) {
        ALOGE("Empty text, do not render");
        if (flush) clear();
        return DFBRect::empty();
    }

    Font font;
    font.family = textParams.fontFamily;
    font.size = textParams.fontSize;

    BoundingBox fontBox = getFontBox(content, videoOriginRect, font);
    ALOGD("fontBox= [%f, %f, %f, %f]", fontBox.x, fontBox.y, fontBox.x2, fontBox.y2);
    if (fontBox.isEmpty()) {
        if (flush) clear();
        ALOGE_IF(strlen(content) > 0, "No support text type rendering");
        return DFBRect::empty();
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
    DFBRect srcRect{(int)fontBox.x, (int)fontBox.y, (int)fontBox.x2, (int)fontBox.y2};

    unsigned char * data = textSurface.data();
    if (!data || !drawImage(type, data, pts, buffer_size, spu_width, spu_height, videoOriginRect, srcRect, dst)) {
        ALOGE("%s, No valid data will to be drew", __FUNCTION__);
        if (flush) clear();
        return DFBRect::empty();
    }

    return srcRect;
}

// save png picture
void DFBDevice::saveAsPNG(const char *filename, uint32_t *data, int width, int height) {
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


#endif // USE_DFB
