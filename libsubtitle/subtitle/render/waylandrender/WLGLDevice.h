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
    const int WIDTH = 1920;
    const int HEIGHT = 1080;

    WLGLDevice();
    ~WLGLDevice();

    bool initCheck() { return mInited; }
    bool init();

    void clear(WLGLRect *rect = nullptr) ;
    void drawColor(float r, float g, float b, float a, bool flush = true);
    void drawColor(float r, float g, float b, float a, WLRect& rect, bool flush = true);
    void drawImage(void *img, WLRect &videoOriginRect, WLRect &src, WLRect &dst, bool flush = true);
    void drawText(const char* content, WLRect &videoOriginRect, WLRect &src, WLRect &dst, bool flush = true);

    WLRect screenRect() { return mScreenRect;}

    struct wl_egl_window* wlEglWindow = nullptr;
private:
    bool initDisplay();
    bool initEGL();
    void setupEnv();
    bool createDisplay();
    static void OnRegistry(void *data, struct wl_registry *registry, uint32_t name,
                                const char *interface, uint32_t version);
    bool initTexture(void* data, WLRect &videoOriginRect, WLRect &cropRect);
    void releaseEGL();
    void releaseWL();
    void glAssert(const char* where = nullptr);

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

    bool mInited = false;
};

#endif //_WLGLDEVICE_H
