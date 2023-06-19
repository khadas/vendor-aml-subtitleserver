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

#define LOG_TAG "WLGLDevice"

#include "WLGLDevice.h"

#include <wayland-client.h>
#include <wayland-egl.h>

#include <cstdlib>
#include <utils/Log.h>
#include <cstring>
#include <vector>
#include <string>

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

WLGLDevice::WLGLDevice() {
    ALOGD("WLGLDevice +++");
    mInited = init();
}

WLGLDevice::~WLGLDevice() {
    ALOGD("WLGLDevice ---");
    releaseEGL();
    releaseWL();
}

bool WLGLDevice::init() {
    return initDisplay();
}

bool WLGLDevice::connectDisplay() {

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

bool WLGLDevice::initDisplay() {
    if (!connectDisplay()) {
        ALOGE("Display init failed");
        return false;
    }

    if (!initEGL()) {
        ALOGE("initEGL failed.");
        return false;
    }
    return true;
}

void WLGLDevice::setupEnv(const char* runtimeDir, const char* waylandDisplay) {
    ALOGD("setupEnv, ENV_XDG_RUNTIME_DIR= %s, ENV_WAYLAND_DISPLAY= %s",
            runtimeDir, waylandDisplay);

    setenv(ENV_XDG_RUNTIME_DIR, runtimeDir, 1);
    setenv(ENV_WAYLAND_DISPLAY, waylandDisplay, 1);
}

bool WLGLDevice::createDisplay() {
    display = wl_display_connect(nullptr);
    if (!display) {
        ALOGE("%s: display connect failed", __FUNCTION__ );
        return false;
    }

    static const struct wl_registry_listener registry_listener = {
            OnRegistry,
            nullptr,
    };

    struct wl_registry *registry = wl_display_get_registry(display);
    if (!registry) {
        ALOGE("%s: wl_display_get_registry failed", __FUNCTION__ );
        return false;
    }
    wl_registry_add_listener(registry, &registry_listener, this);

    ALOGD("wl_display_roundtrip 111");
    wl_display_roundtrip(display);
    wl_registry_destroy(registry);
    ALOGD("wl_display_roundtrip 222");
    return true;
}

void WLGLDevice::OnRegistry(void *data, struct wl_registry *registry, uint32_t name,
                          const char *interface, uint32_t /*version*/) {
    auto* wl = static_cast<WLGLDevice *>(data);
    int len= strlen(interface);
    if (len == 13 && !strncmp(interface, "wl_compositor", len)) {
        wl->compositor =
                (struct wl_compositor*)wl_registry_bind(registry, name, &wl_compositor_interface, 1);
    } else if ((len==8) && !strncmp(interface, "wl_shell", len)) {
        wl->shell = (struct wl_shell*)wl_registry_bind(registry, name, &wl_shell_interface, 1);
    }
}

static void shellSurfacePing(void *data, struct wl_shell_surface *shell_surface, uint32_t serial) {
    wl_shell_surface_pong(shell_surface, serial);
}

static void shellSurfaceConfigure(void *data, struct wl_shell_surface *shell_surface,
                                    uint32_t edges, int32_t width, int32_t height) {
    auto* device = static_cast<WLGLDevice*>(data);
    wl_egl_window_resize(device->wlEglWindow, width, height, 0, 0);
}

static struct wl_shell_surface_listener shell_surface_listener = {
        &shellSurfacePing,
        &shellSurfaceConfigure,
        nullptr,
};

void WLGLDevice::getScreenSize(size_t *width, size_t *height) {
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

    {// From EGL
        EGLint w, h;
        eglQuerySurface(display, surface, EGL_WIDTH, &w);
        eglQuerySurface(display, surface, EGL_HEIGHT, &h);

        ALOGD("getScreenSize, from eglQuerySurface: [%dx%d]", w, h);

        if ((w > 0) && (h > 0)) {
            *width = w;
            *height = h;
            return;
        }
    }

    ALOGD("getScreenSize, from defult: [%dx%d]", WIDTH, HEIGHT);
    *width = WIDTH;
    *height = HEIGHT;
}

bool WLGLDevice::initEGL() {
    EGLConfig config;
    EGLBoolean ret;
    EGLint config_attribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 0,
            EGL_NONE
    };

    eglDisplay = eglGetDisplay(display);
    if (!eglDisplay) {
        ALOGE("eglGetDisplay failed");
        return false;
    }

    ret = eglInitialize(eglDisplay, nullptr, nullptr);
    if (!ret) {
        ALOGE("eglInitialize failed");
        return false;
    }

    EGLint config_nums;
    ret = eglChooseConfig(eglDisplay, config_attribs, &config, 1, &config_nums);
    if (!ret || config_nums < 1) {
        ALOGE("eglChooseConfig failed");
        return false;
    }

    eglContext = eglCreateContext(eglDisplay, config, NULL, NULL);
    if (!eglContext) {
        ALOGE("eglCreateContext failed");
        return false;
    }

    size_t width, height;
    getScreenSize(&width, &height);
    mScreenRect.set(0, 0, width, height);
    mScreenGLRect.set(0, height, width, 0);

    ALOGD("getScreenSize: [%d, %d]", (int)width, (int)height);

    surface = wl_compositor_create_surface(compositor);
    shellSurface = wl_shell_get_shell_surface(shell, surface);
    wl_shell_surface_add_listener(shellSurface, &shell_surface_listener, this);
    //wl_shell_surface_set_toplevel(shellSurface);

    wlEglWindow = wl_egl_window_create (surface, width, height);
    eglSurface = eglCreateWindowSurface(eglDisplay, config, wlEglWindow, NULL);
    if (!eglSurface) {
        ALOGE("eglCreateWindowSurface failed");
        return false;
    }

    if (eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext) == EGL_FALSE) {
        ALOGE("eglMakeCurrent failed");
        return false;
    }

    glViewport(0, 0, width, height);
    glTexEnvx(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glEnable(GL_TEXTURE_2D);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    ALOGD("initEGL OK");
    return true;
}

bool WLGLDevice::initTexture(void* data, WLRect &videoOriginRect, WLRect &cropRect) {
    ALOGD("sourceCrop glRect= [%d, %d, %d, %d]", cropRect.x(), cropRect.y(),
            cropRect.width(), cropRect.height());
    if (data == nullptr || cropRect.isEmpty())
        return false;

    if (!isTextMultiPart) {
        if (mTexture.texture >= 0) {
            glDeleteTextures(1, &(mTexture.texture));
            glAssert("initTexture_glDeleteTextures");
        }
        glGenTextures(1, &(mTexture.texture));
        glAssert("initTexture_glGenTextures");
    }
    //mTexture.crop.set(videoOriginRect);

    WLGLRect glRect(videoOriginRect, mScreenGLRect);
    ALOGD("videoOriginRect glRect= [%d, %d, %d, %d]", glRect.x(), glRect.y(), glRect.width(), glRect.height());
    GLint crop[4] = { glRect.x(), glRect.height(), glRect.width(), -glRect.height() };

    //Video original crop
    glBindTexture(GL_TEXTURE_2D, mTexture.texture);
    glAssert("initTexture_glBindTexture");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, glRect.width(), glRect.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glAssert("initTexture_glTexImage2D");

    // Avoid data is out of display frame
    WLRect subCrop;
    if (!videoOriginRect.intersect(cropRect, &subCrop)) {
        ALOGE("Final subCrop is empty, return");
        return false;
    }
    subCrop.log("Final subCrop");

    //Subtitle rect
    glTexSubImage2D(GL_TEXTURE_2D, 0, subCrop.x(), subCrop.y(),
                    subCrop.width(), subCrop.height(), GL_RGBA, GL_UNSIGNED_BYTE, data);
    glAssert("initTexture_glTexSubImage2D");

    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, crop);
    glAssert("initTexture_GL_TEXTURE_CROP_RECT_OES");
    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glAssert("initTexture_GL_TEXTURE_MIN_FILTER");
    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glAssert("initTexture_GL_TEXTURE_MAG_FILTER");
    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glAssert("initTexture_GL_TEXTURE_WRAP_S");
    glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glAssert("initTexture_GL_TEXTURE_WRAP_T");
    return true;
}

void WLGLDevice::releaseEGL() {
    glDisable(GL_BLEND);

    if (mTexture.texture >= 0) {
        glDeleteTextures(1, &(mTexture.texture));
    }

    if (eglDisplay != nullptr) {
        eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (eglSurface != nullptr) {
            eglDestroySurface(eglDisplay, eglSurface);
            eglSurface = nullptr;
        }
        if (eglContext != nullptr) {
            eglDestroyContext(eglDisplay, eglContext);
            eglContext = nullptr;
        }
        eglTerminate(eglDisplay);
        eglReleaseThread();
        eglDisplay = nullptr;
    }
}

void WLGLDevice::releaseWL() {
    if (wlEglWindow != nullptr) {
        wl_egl_window_destroy(wlEglWindow);
        wlEglWindow = nullptr;
    }

    if (shellSurface != nullptr) {
        wl_shell_surface_destroy(shellSurface);
        shellSurface = nullptr;
    }
    if (surface != nullptr) {
        wl_surface_destroy(surface);
        surface = nullptr;
    }

    if (compositor != nullptr) {
        wl_compositor_destroy(compositor);
        compositor = nullptr;
    }

    if (shell != nullptr) {
        wl_shell_destroy(shell);
        shell = nullptr;
    }

    if (display != nullptr) {
        wl_display_disconnect(display);
        display = nullptr;
    }
}

void WLGLDevice::drawColor(float r, float g, float b, float a, bool flush) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);

    if (flush) {
        eglSwapBuffers(eglDisplay, eglSurface);
        glAssert("drawColor_1_eglSwapBuffers");
    }
}

void WLGLDevice::drawColor(float r, float g, float b, float a, WLRect &rect, bool flush) {
    WLRect intersectRect = WLRect();
    if (rect.isEmpty() || !mScreenRect.intersect(rect, &intersectRect)) {
        ALOGW("%s, Rect checked failed", __FUNCTION__ );
        return;
    }

    WLGLRect glRect(intersectRect, mScreenGLRect);
    glEnable(GL_SCISSOR_TEST);
    glScissor(glRect.x(), glRect.y(),glRect.width(), glRect.height());
    glAssert("drawColor_glScissor");
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);

    if (flush) {
        eglSwapBuffers(eglDisplay, eglSurface);
        glAssert("drawColor_2_eglSwapBuffers");
    }
}

bool WLGLDevice::drawImage(void *img, WLRect &videoOriginRect, WLRect &src, WLRect &dst, bool flush) {
    if (!initTexture(img, videoOriginRect, src)) {
        ALOGE("%s: initTexture failed", __FUNCTION__ );
        return false;
    }

    WLGLRect targetRect(dst, mScreenGLRect);
    ALOGV("drawImage, targetRect= [%d, %d, %d, %d]", targetRect.x(), targetRect.y(),
          targetRect.width(), targetRect.height());

    clear();

    glEnable(GL_SCISSOR_TEST);
    glScissor(targetRect.x(), targetRect.y(), targetRect.width(), targetRect.height());

    glDrawTexiOES(targetRect.x(), targetRect.y(), 0, targetRect.width(), targetRect.height());
    glAssert("drawImage_glDrawTexiOES");

    glDisable(GL_SCISSOR_TEST);

    if (flush) {
        eglSwapBuffers(eglDisplay, eglSurface);
        glAssert("drawImage_eglSwapBuffers");
    }

    return true;
}

void WLGLDevice::clear(WLGLRect * rect) {
    if (rect == nullptr) {
        //CLear all screen
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    } else {
        glEnable(GL_SCISSOR_TEST);
        glScissor(rect->x(), rect->y(), rect->width(), rect->height());
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);
    }
}

static BoundingBox getFontBox(const char *content, WLRect &surfaceRect, Font& font) {
    Surface textSurface(surfaceRect.width(), surfaceRect.height());
    DrawContext drawContext(&textSurface);
    Text tmp(&drawContext, 0, 0, content, font);
    BoundingBox fontBox = tmp.calculateBounds(Pen(Colors::White));
    return fontBox;
}

void WLGLDevice::drawMultiText(TextParams &textParams, WLRect &videoOriginRect,
                               WLRect &src, WLRect &dst) {
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

            WLRect textRect = drawText(textParams, videoOriginRect, src, dst,
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

WLRect WLGLDevice::drawText(TextParams& textParams, WLRect &videoOriginRect,
                          WLRect &/*src*/, WLRect &dst, int marginBottom, bool flush) {

    const char* content = textParams.content;
    if (content == nullptr || strlen(content) <= 0) {
        ALOGE("Empty text, do not render");
        if (flush) clear();
        return WLRect::empty();
    }

    Font font;
    font.family = textParams.fontFamily;
    font.size = textParams.fontSize;

    BoundingBox fontBox = getFontBox(content, videoOriginRect, font);
    ALOGD("fontBox= [%f, %f, %f, %f]", fontBox.x, fontBox.y, fontBox.x2, fontBox.y2);
    if (fontBox.isEmpty()) {
        if (flush) clear();
        ALOGE_IF(strlen(content) > 0, "No support text type rendering");
        return WLRect::empty();
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
    WLRect srcRect{(int)fontBox.x, (int)fontBox.y, (int)fontBox.x2, (int)fontBox.y2};

    uint8_t * data = textSurface.data();
    if (!data || !drawImage(data, videoOriginRect, srcRect, dst, flush)) {
        ALOGE("%s, No valid data will to be drew", __FUNCTION__);
        if (flush) clear();
        return WLRect::empty();
    }

    return srcRect;
}

void WLGLDevice::glAssert(const char* where) {
    GLenum err = glGetError();

    if (where == nullptr) {
        ALOGE_IF(err != GL_NO_ERROR, "GL error(err_code: %d): [%s]", err, __FUNCTION__);
    } else {
        ALOGE_IF(err != GL_NO_ERROR, "GL error(err_code: %d): [%s]", err, where);
    }

}

#endif // USE_WAYLAND
