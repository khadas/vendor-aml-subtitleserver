#define LOG_TAG "WLRender"

#include "WLRender.h"

#include <thread>
#include <utils/Log.h>

#include "WLGLDevice.h"

WLRender* WLRender::sInstance = nullptr;

WLRender *WLRender::GetInstance() {
    if (sInstance == nullptr) {
        sInstance = new WLRender();
    }
    return sInstance;
}

void WLRender::Clear() {
    if (sInstance != nullptr) {
        delete sInstance;
        sInstance = nullptr;
    }
}

WLRender::WLRender() {
    ALOGD("WLRender +++");
    std::thread thread(WLRender::threadFunc, this);
    thread.detach();
}

WLRender::~WLRender() {
    ALOGD("WLRender ---");
    requestExit();
}

void WLRender::threadFunc(void *data) {
    ALOGD("WLRender threadFunc start");
    WLRender* render = static_cast<WLRender *>(data);

    if (render->mLooper == nullptr) {
        render->mHandler = new AMLHandler(render);
        render->mLooper = android::Looper::prepare(0);

        render->sendMessage(android::Message(kWhat_thread_init));
    }

    while (!render->mRequestExit) {
        render->mLooper->pollOnce(-1);
    }

    render->onThreadExit();
    ALOGD("WLRender thread exit !!!");
}

void WLRender::AMLHandler::handleMessage(const AMLMessage &message) {
    ALOGD("handleMessage: %d", message.what);
    WLRender* render = mWk_WLRender;
    if (mWk_WLRender == nullptr) {
        return;
    }

    switch (message.what) {
        case kWhat_thread_init:
            render->wlInit();
            break;
        case kWhat_show_subtitle_item:
            render->drawItems();
            break;
        case kWhat_clear:
            //Clear screen
            render->clearScreen();
            break;
    }
}

void WLRender::requestExit() {
    if (mRequestExit)
        return;

    mRequestExit = true;
    if (mLooper->isPolling()) {
        mLooper->wake();
    }
}

void WLRender::wlInit() {
    mWLDevice = std::make_shared<WLGLDevice>();
    if (!mWLDevice->initCheck()) {
        ALOGE("wlInit, initCheck failed");
        mLooper->removeMessages(mHandler);
        requestExit();
        return;
    }

    sendMessage(AMLMessage(kWhat_clear));
    ALOGD("RenderDevice init OK.");
}

bool WLRender::showSubtitleItem(std::shared_ptr<AML_SPUVAR> spu, int type) {
    mRenderMutex.lock();
    mShowingSubs.push_back(spu);
    mRenderMutex.unlock();

    mParseType = type;
    sendMessage(AMLMessage(kWhat_show_subtitle_item));
    return true;
}

bool WLRender::hideSubtitleItem(std::shared_ptr<AML_SPUVAR> spu) {
    mRenderMutex.lock();
    mShowingSubs.remove(spu);
    mRenderMutex.unlock();

    sendMessage(AMLMessage(kWhat_show_subtitle_item));
    return true;
}

void WLRender::resetSubtitleItem() {
    mRenderMutex.lock();
    mShowingSubs.clear();
    mRenderMutex.unlock();

    sendMessage(AMLMessage(kWhat_clear));
}

void WLRender::removeSubtitleItem(std::shared_ptr<AML_SPUVAR> spu) {
    mRenderMutex.lock();
    mShowingSubs.remove(spu);
    mRenderMutex.unlock();
}

void WLRender::drawItems() {
    android::AutoMutex _l(mRenderMutex);

    if (mShowingSubs.empty()) {
        ALOGW("No any item can be draw.");
        clearScreen();
        return;
    }

    for (auto it = mShowingSubs.begin(); it != mShowingSubs.end(); it++) {
        if ((*it) == nullptr) continue;

        uint16_t x = (*it)->spu_start_x;
        uint16_t y = (*it)->spu_start_y;
        uint16_t w = (*it)->spu_width;
        uint16_t h = (*it)->spu_height;

        uint16_t dsp_w = (*it)->spu_origin_display_w;
        uint16_t dsp_h = (*it)->spu_origin_display_h;

        WLRect rect = WLRect(x, y, x + w, y + h);
        WLRect originDisplayRect = WLRect(0, 0, dsp_w, dsp_h);

        WLRect screenRect = mWLDevice->screenRect();
        if ((*it)->isSimpleText) {
            ALOGD("Text type");
            auto text = reinterpret_cast<const char *>((*it)->spu_data);
            mWLDevice->drawText(text, originDisplayRect, rect, screenRect);
        } else {
            ALOGD("Image type");
            mWLDevice->drawImage((*it)->spu_data, originDisplayRect, rect, screenRect);
        }
    }

    ALOGD("After draw items: %d", mShowingSubs.size());
}

void WLRender::onThreadExit() {
    ALOGD("%s", __FUNCTION__ );
    mWLDevice.reset();
}

void WLRender::clearScreen() {
    ALOGD("clearScreen");
    mWLDevice->drawColor(0, 0, 0, 0);
}

void WLRender::sendMessage(const AMLMessage &message, nsecs_t nsecs) {
    if (nsecs <= 0) {
        mLooper->sendMessage(mHandler, message);
    } else {
        mLooper->sendMessageDelayed(nsecs, mHandler, message);
    }
}
