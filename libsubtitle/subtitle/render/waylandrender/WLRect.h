#ifndef _WLRECT_H
#define _WLRECT_H

#include <cstdint>
#include <utils/Log.h>

class WLGLRect;

///////////////////////////// For normal(UI) coordinates  ////////////////////////////
class WLRect {
public:
    WLRect() {};
    WLRect(int left, int top, int right, int bottom)
            : mLeft(left), mTop(top), mRight(right), mBottom(bottom) {}

    void set(WLRect &rect) {
        mLeft = rect.mLeft;
        mTop = rect.mTop;
        mRight = rect.mRight;
        mBottom = rect.mBottom;
    }

    void set(int left, int top, int right, int bottom) {
        mLeft = left;
        mTop = top;
        mRight = right;
        mBottom = bottom;
    }

    bool operator==(WLRect &in) {
        return mLeft == in.mLeft && mTop == in.mTop
        && mRight == in.mRight && mBottom == in.mBottom;
    }

    bool operator!=(WLRect &in) {
        return !operator==(in);
    }


    static inline int32_t min(int32_t a, int32_t b) {
        return (a < b) ? a : b;
    }

    static inline int32_t max(int32_t a, int32_t b) {
        return (a > b) ? a : b;
    }

    bool isEmpty() {
        return width() <= 0 || height() <= 0;
    }

    bool intersect(const WLRect &rect, WLRect* out) {
        out->mLeft = max(mLeft, rect.mLeft);
        out->mTop = max(mTop, rect.mTop);
        out->mRight = min(mRight, rect.mRight);
        out->mBottom = min(mBottom, rect.mBottom);
        return !(out->isEmpty());
    }

    int x() { return mLeft; }

    int y() { return mTop; }

    int width() { return mRight - mLeft; }

    int height() { return mBottom - mTop; }

    void log(const char * msg) {
        ALOGD("%s: [l=%d, t=%d, r=%d, b=%d] [w=%d, h=%d]", msg,
                mLeft, mTop, mRight, mBottom, width(), height());
    }

private:
    friend class WLGLRect;
    int mLeft, mTop, mRight, mBottom;
};

///////////////////////////// For GLES coordinates  ////////////////////////////
class WLGLRect {
public:
    WLGLRect() = default;
     WLGLRect(WLRect & rect, WLGLRect & refRect){
        mLeft = rect.mLeft;
        mTop = refRect.height() - rect.mTop;
        mRight = rect.mRight;
        mBottom = refRect.height() - rect.mBottom;
    }

    void set(int left, int top, int right, int bottom) {
        mLeft = left;
        mTop = top;
        mRight = right;
        mBottom = bottom;
    }

    int x() { return mLeft; }

    int y() { return mBottom; }

    int width() { return mRight - mLeft; }

    int height() { return mTop - mBottom; }

private:
    int mLeft, mTop, mRight, mBottom;
};
#endif //_WLRECT_H
