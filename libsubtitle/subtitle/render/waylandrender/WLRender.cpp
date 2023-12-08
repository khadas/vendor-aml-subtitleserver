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

#define LOG_TAG "WLRender"

#include "WLRender.h"

#include <thread>
#include "SubtitleLog.h"

#include "WLGLDevice.h"
#include <Parser.h>
#include <CCJsonParser.h>

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
    SUBTITLE_LOGI("WLRender +++");
    std::thread thread(WLRender::threadFunc, this);
    thread.detach();
}

WLRender::~WLRender() {
    SUBTITLE_LOGI("WLRender ---");
    requestExit();
}

void WLRender::threadFunc(void *data) {
    SUBTITLE_LOGI("WLRender threadFunc start");
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
    SUBTITLE_LOGI("WLRender thread exit !!!");
}

void WLRender::AMLHandler::handleMessage(const AMLMessage &message) {
    SUBTITLE_LOGI("handleMessage: %d", message.what);
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
        SUBTITLE_LOGE("wlInit, initCheck failed");
        mLooper->removeMessages(mHandler);
        requestExit();
        return;
    }

    sendMessage(AMLMessage(kWhat_clear));
    SUBTITLE_LOGI("RenderDevice init OK.");
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

    //sendMessage(AMLMessage(kWhat_show_subtitle_item));
}

void WLRender::drawItems() {
    android::AutoMutex _l(mRenderMutex);

    if (mShowingSubs.empty()) {
        SUBTITLE_LOGE("No any item can be draw.");
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
        if (isText(*it)) {
            auto text = reinterpret_cast<const char *>((*it)->spu_data);

            if ((*it)->subtitle_type == TYPE_SUBTITLE_CLOSED_CAPTION) {
                std::vector<std::string> ccData;
                std::string input = text;
                if (!CCJsonParser::populateData(input, ccData)) {
                    SUBTITLE_LOGE("No valid data");
                    continue;
                }

                std::string ccText;
                for (std::string& item : ccData) {
                    ccText.append(item).append("\n");
                }

                text = ccText.c_str();
            }


            SUBTITLE_LOGI("Text type: '%s'", text);

            // Using 720p to show subtitle for reduce mem.
            originDisplayRect.set(0, 0, 1280, 720);

            WLGLDevice::TextParams textParams;
            textParams.content = text;
            textParams.fontFamily = "Liberation Sans";
            textParams.fontSize = 30;
            textParams.textFillColor = Cairo::Colors::White;
            textParams.textLineColor = Cairo::Colors::White;
            textParams.bgPadding = 10;
            textParams.bgColor = Cairo::Colors::GrayTransparent;

            mWLDevice->drawMultiText(textParams, originDisplayRect, rect, screenRect);
        } else {
            SUBTITLE_LOGI("Image type");

            // Show full screen for Teletext
            bool showFullScreen =
                    (*it)->subtitle_type == SubtitleType::TYPE_SUBTITLE_DVB_TELETEXT
                    || (*it)->subtitle_type == SubtitleType::TYPE_SUBTITLE_DVB_TELETEXT;
            //mWLDevice->drawColor(0, 0, 0, 0);
            mWLDevice->drawImage((*it)->spu_data, showFullScreen ? rect : originDisplayRect, rect, screenRect);
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
    }

    SUBTITLE_LOGI("After draw items: %d", mShowingSubs.size());
}

bool WLRender::isText(std::shared_ptr<AML_SPUVAR> &spu) {
    return spu->isSimpleText
           || spu->subtitle_type == SubtitleType::TYPE_SUBTITLE_CLOSED_CAPTION
           || spu->subtitle_type == SubtitleType::TYPE_SUBTITLE_SCTE27
           || spu->subtitle_type == SubtitleType::TYPE_SUBTITLE_ARIB_B24
           || spu->subtitle_type == SubtitleType::TYPE_SUBTITLE_DVB_TTML;
}

void WLRender::onThreadExit() {
    SUBTITLE_LOGI("%s", __FUNCTION__ );
    mWLDevice.reset();
}

void WLRender::clearScreen() {
    SUBTITLE_LOGI("clearScreen");
    mWLDevice->drawColor(0, 0, 0, 0);
}

void WLRender::sendMessage(const AMLMessage &message, nsecs_t nsecs) {
    if (nsecs <= 0) {
        mLooper->sendMessage(mHandler, message);
    } else {
        mLooper->sendMessageDelayed(nsecs, mHandler, message);
    }
}
