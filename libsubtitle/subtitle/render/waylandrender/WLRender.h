#ifndef _WLOPENGLRENDER_H
#define _WLOPENGLRENDER_H

#include "Render.h"

#include <list>
#include <utils/Looper.h>
#include <utils/StrongPointer.h>

using AMLooper = android::sp<android::Looper>;
using AMLMessage = android::Message;

class WLGLDevice;

class WLRender {
public:

    static WLRender* GetInstance();
    static void Clear();

    void requestExit();

    bool showSubtitleItem(std::shared_ptr<AML_SPUVAR> spu, int type);

    bool hideSubtitleItem(std::shared_ptr<AML_SPUVAR> spu);

    void resetSubtitleItem();

    void removeSubtitleItem(std::shared_ptr<AML_SPUVAR> spu);

private:
    WLRender();
    virtual ~WLRender();

    static void threadFunc(void *data);

    void onThreadExit();

    void sendMessage(const AMLMessage &message, nsecs_t nsecs = 0);

    void wlInit();

    void drawItems();

    void clearScreen();

    enum {
        kWhat_thread_init,
        kWhat_show_subtitle_item,
        kWhat_clear,
    };

    class AMLHandler : public android::MessageHandler {
    public:
        AMLHandler(WLRender* wlRender)
                : mWk_WLRender(wlRender) {
        };

        virtual void handleMessage(const AMLMessage &message) override;

    private:
        WLRender* mWk_WLRender;
    };

    enum Color {
        transparent = 0x00000000,
        black = 0xff0000ff,
    };

private:
    int mParseType;
    bool mRequestExit = false;

    AMLooper mLooper;
    android::sp<AMLHandler> mHandler;
    std::shared_ptr<WLGLDevice> mWLDevice;
    std::list<std::shared_ptr<AML_SPUVAR>> mShowingSubs;

    static WLRender* sInstance;
};

/*Holding the global WLRender obj*/
class WLRenderWrapper : public Render {
public:
    WLRenderWrapper()
    :mWLRender(WLRender::GetInstance()){}

    ~WLRenderWrapper(){
        /*Do not delete*/
        mWLRender = nullptr;
    }

    virtual RenderType getType() { return DIRECT_RENDER; }

    virtual bool showSubtitleItem(std::shared_ptr<AML_SPUVAR> spu, int type) {
        if (mWLRender)
            mWLRender->showSubtitleItem(spu, type);

        return true;
    };

    virtual bool hideSubtitleItem(std::shared_ptr<AML_SPUVAR> spu) {
        if (mWLRender)
            mWLRender->hideSubtitleItem(spu);
        return true;
    };

    virtual void resetSubtitleItem() {
        if (mWLRender)
            mWLRender->resetSubtitleItem();
    };

    virtual void removeSubtitleItem(std::shared_ptr<AML_SPUVAR> spu) {
        if (mWLRender)
            mWLRender->removeSubtitleItem(spu);
    };

private:
    WLRender* mWLRender = nullptr;
};


#endif //_WLOPENGLRENDER_H
