#define LOG_TAG "WLGLDevice"

#include "WLGLDevice.h"

#include <wayland-client.h>
#include <wayland-egl.h>

#include <cstdlib>
#include <utils/Log.h>
#include <cstring>

#include "textrender/text.h"
#include "textrender/rectangle.h"
using namespace Cairo;

#define ENV_XDG_RUNTIME_DIR "XDG_RUNTIME_DIR"
#define ENV_WAYLAND_DISPLAY "WAYLAND_DISPLAY"

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

bool WLGLDevice::initDisplay() {
    if (!createDisplay()) {
        ALOGE("createDisplay failed.");
        return false;
    }

    if (!initEGL()) {
        ALOGE("initEGL failed.");
        return false;
    }
    return true;
}

void WLGLDevice::setupEnv() {
    ALOGD("setupEnv");
    setenv(ENV_XDG_RUNTIME_DIR, "/run/user/0", 1);
    setenv(ENV_WAYLAND_DISPLAY, "wayland-0", 1);
}

bool WLGLDevice::createDisplay() {
    setupEnv();

    display = wl_display_connect(NULL);
    if (display == NULL) {
        ALOGE("%s: display connect failed", __FUNCTION__ );
        return false;
    }

    static const struct wl_registry_listener registry_listener = {
            OnRegistry,
            NULL,
    };

    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, this);

    wl_display_roundtrip(display);
    wl_registry_destroy(registry);
    return true;
}

void WLGLDevice::OnRegistry(void *data, struct wl_registry *registry, uint32_t name,
                          const char *interface, uint32_t version) {
    WLGLDevice* wl = static_cast<WLGLDevice *>(data);
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        wl->compositor =
                (struct wl_compositor*)wl_registry_bind(registry, name,
                                                        &wl_compositor_interface, 1);
    } else if (strcmp(interface, wl_shell_interface.name) == 0) {
        wl->shell = (struct wl_shell*)wl_registry_bind(registry, name, &wl_shell_interface, 1);
    }
}

static void shellSurfacePing(void *data, struct wl_shell_surface *shell_surface, uint32_t serial) {
    wl_shell_surface_pong(shell_surface, serial);
}

static void shellSurfaceConfigure(void *data, struct wl_shell_surface *shell_surface,
                                    uint32_t edges, int32_t width, int32_t height) {
    WLGLDevice* device = static_cast<WLGLDevice*>(data);
    wl_egl_window_resize(device->wlEglWindow, width, height, 0, 0);
}

static struct wl_shell_surface_listener shell_surface_listener = {
        &shellSurfacePing,
        &shellSurfaceConfigure,
        NULL,
};

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

    EGLint width, height;
    eglQuerySurface(display, surface, EGL_WIDTH, &width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &height);

    if (width <= 0 || height<= 0) {
        width = WIDTH;
        height = HEIGHT;
    }

    mScreenRect.set(0, 0, width, height);
    mScreenGLRect.set(0, height, width, 0);

    surface = wl_compositor_create_surface(compositor);
    shellSurface = wl_shell_get_shell_surface(shell, surface);
    wl_shell_surface_add_listener(shellSurface, &shell_surface_listener, this);
    wl_shell_surface_set_toplevel(shellSurface);

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
    return true;
}

bool WLGLDevice::initTexture(void* data, WLRect &videoOriginRect, WLRect &cropRect) {
    ALOGD("sourceCrop glRect= [%d, %d, %d, %d]", cropRect.x(), cropRect.y(),
            cropRect.width(), cropRect.height());
    if (data == nullptr || cropRect.isEmpty())
        return false;

    if (mTexture.texture >= 0) {
        glDeleteTextures(1, &(mTexture.texture));
        glAssert("initTexture_glDeleteTextures");
    }
    glGenTextures(1, &(mTexture.texture));
    glAssert("initTexture_glGenTextures");
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

    //Subtitle rect
    glTexSubImage2D(GL_TEXTURE_2D, 0, cropRect.x(), cropRect.y(),
                    cropRect.width(), cropRect.height(), GL_RGBA, GL_UNSIGNED_BYTE, data);
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

void WLGLDevice::drawImage(void *img, WLRect &videoOriginRect, WLRect &src, WLRect &dst, bool flush) {
    if (!initTexture(img, videoOriginRect, src)) {
        ALOGE("%s: initTexture failed", __FUNCTION__ );
        return;
    }

    WLGLRect targetRect(dst, mScreenGLRect);

    clear();

//    glEnable(GL_SCISSOR_TEST);
//    glScissor(glRect.x(), glRect.y(), glRect.width(), glRect.height());
//    glEnable(GL_BLEND);
//    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDrawTexiOES(targetRect.x(), targetRect.y(), 0, targetRect.width(), targetRect.height());
    glAssert("drawImage_glDrawTexiOES");

//    glDisable(GL_BLEND);
//    glDisable(GL_SCISSOR_TEST);

    if (flush) {
        eglSwapBuffers(eglDisplay, eglSurface);
        glAssert("drawImage_eglSwapBuffers");
    }
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

void WLGLDevice::drawText(const char *content, WLRect &videoOriginRect,
                          WLRect &src, WLRect &dst, bool flush) {
    Surface textSurface(src.width(), src.height());
    DrawContext drawContext(&textSurface);
    Font font;
    font.family = "Liberation Sans"; //TODO: Support more family
    font.size = 20;//TODO: Use flexible font size

    Rectangle rectangle(&drawContext, 0, 0, textSurface.getWidth(), textSurface.getHeight());
    rectangle.draw(Pen(Colors::Black), Pen(Colors::Black));

    Text text(&drawContext, 0, 0, content, font);
    BoundingBox box = {};
    box.x = 0;
    box.y = 0;
    box.setWidth(textSurface.getWidth());
    box.setHeight(textSurface.getHeight());
    text.drawCenter(box, Pen(Colors::White));

    uint8_t * data = textSurface.data();
    if (data) {
        drawImage(data, videoOriginRect, src, dst, flush);
    }
}

void WLGLDevice::glAssert(const char* where) {
    GLenum err = glGetError();

    if (err != GL_NO_ERROR) {
        ALOGD("GL error: GL error(err_code: %d) on [%s]", err, where);
    }

    if (where == nullptr) {
        ALOG_ASSERT(err == GL_NO_ERROR, "GL error(err_code: %d): [%s]", err, __FUNCTION__);
    } else {
        ALOG_ASSERT(err == GL_NO_ERROR, "GL error(err_code: %d): [%s]", err, where);
    }

}
