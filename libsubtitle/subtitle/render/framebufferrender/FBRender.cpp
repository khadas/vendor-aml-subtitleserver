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

#define LOG_TAG "FBRender"

#include "FBRender.h"

#include <thread>
#include <utils/Log.h>

#include "FBDevice.h"
#include <Parser.h>
#include <CCJsonParser.h>

FBRender* FBRender::sInstance = nullptr;

FBRender *FBRender::GetInstance() {
    if (sInstance == nullptr) {
        sInstance = new FBRender();
    }
    return sInstance;
}

void FBRender::Clear() {
    if (sInstance != nullptr) {
        delete sInstance;
        sInstance = nullptr;
    }
}

FBRender::FBRender() {
    ALOGD("FBRender +++");
    std::thread thread(FBRender::threadFunc, this);
    thread.detach();
}

FBRender::~FBRender() {
    ALOGD("FBRender ---");
    requestExit();
}

void FBRender::threadFunc(void *data) {
    ALOGD("FBRender threadFunc start");
    FBRender* render = static_cast<FBRender *>(data);

    if (render->mLooper == nullptr) {
        render->mHandler = new AMLHandler(render);
        render->mLooper = android::Looper::prepare(0);

        render->sendMessage(android::Message(kWhat_thread_init));
    }

    while (!render->mRequestExit) {
        render->mLooper->pollOnce(-1);
    }

    render->onThreadExit();
    ALOGD("FBRender thread exit !!!");
}

void FBRender::AMLHandler::handleMessage(const AMLMessage &message) {
    ALOGD("handleMessage: %d", message.what);
    FBRender* render = mWk_FBRender;
    if (mWk_FBRender == nullptr) {
        return;
    }

    switch (message.what) {
        case kWhat_thread_init:
            render->fbInit();
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

void FBRender::requestExit() {
    if (mRequestExit)
        return;

    mRequestExit = true;
    if (mLooper->isPolling()) {
        mLooper->wake();
    }
}

void FBRender::fbInit() {
    mFBDevice = std::make_shared<FBDevice>();
    if (!mFBDevice->initCheck()) {
        ALOGE("fbInit, initCheck failed");
        mLooper->removeMessages(mHandler);
        requestExit();
        return;
    }

    sendMessage(AMLMessage(kWhat_clear));
    ALOGD("RenderDevice init OK.");
}

bool FBRender::showSubtitleItem(std::shared_ptr<AML_SPUVAR> spu, int type) {
    mRenderMutex.lock();
    mShowingSubs.push_back(spu);
    mRenderMutex.unlock();

    mParseType = type;
    sendMessage(AMLMessage(kWhat_show_subtitle_item));
    return true;
}

bool FBRender::hideSubtitleItem(std::shared_ptr<AML_SPUVAR> spu) {
    mRenderMutex.lock();
    mShowingSubs.remove(spu);
    mRenderMutex.unlock();

    sendMessage(AMLMessage(kWhat_show_subtitle_item));
    return true;
}

void FBRender::resetSubtitleItem() {
    mRenderMutex.lock();
    mShowingSubs.clear();
    mRenderMutex.unlock();

    sendMessage(AMLMessage(kWhat_clear));
}

void FBRender::removeSubtitleItem(std::shared_ptr<AML_SPUVAR> spu) {
    mRenderMutex.lock();
    mShowingSubs.remove(spu);
    mRenderMutex.unlock();

    sendMessage(AMLMessage(kWhat_show_subtitle_item));
}

void FBRender::drawItems() {
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

        FBRect rect = FBRect(x, y, x + w, y + h);
        FBRect originDisplayRect = FBRect(0, 0, dsp_w, dsp_h);

        FBRect screenRect = mFBDevice->screenRect();
        if (isText(*it)) {
            auto text = reinterpret_cast<const char *>((*it)->spu_data);

            if ((*it)->subtitle_type == TYPE_SUBTITLE_CLOSED_CAPTION) {
                std::vector<std::string> ccData;
                std::string input = text;
                if (!CCJsonParser::populateData(input, ccData)) {
                    ALOGE("No valid data");
                    continue;
                }

                std::string ccText;
                for (std::string& item : ccData) {
                    ccText.append(item).append("\n");
                }

                text = ccText.c_str();
            }


            ALOGD("Text type: '%s'", text);

            // Using 720p to show subtitle for reduce mem.
            originDisplayRect.set(0, 0, 1280, 720);

            FBDevice::TextParams textParams;
            textParams.content = text;
            textParams.fontFamily = "";
            textParams.fontSize = 20;
            textParams.textFillColor = Cairo::Colors::White;
            textParams.textLineColor = Cairo::Colors::White;
            textParams.bgPadding = 10;
            textParams.bgColor = Cairo::Colors::GrayTransparent;

            mFBDevice->drawMultiText((*it)->subtitle_type, textParams,(*it)->pts, (*it)->buffer_size, (*it)->spu_width, (*it)->spu_height, originDisplayRect, rect, screenRect);
        } else {
            ALOGD("Image type");

            // Show full screen for Teletext
            bool showFullScreen =
                    (*it)->subtitle_type == SubtitleType::TYPE_SUBTITLE_DTVKIT_TELETEXT
                    || (*it)->subtitle_type == SubtitleType::TYPE_SUBTITLE_DVB_TELETEXT;

//            mFBDevice->drawImage((*it)->subtitle_type, (*it)->spu_data, (*it)->pts, (*it)->buffer_size, (*it)->spu_width, (*it)->spu_height, showFullScreen ? rect : originDisplayRect, rect, screenRect);
            mFBDevice->drawImage((*it)->subtitle_type, (*it)->spu_data, (*it)->pts, (*it)->buffer_size, (*it)->spu_start_x, (*it)->spu_start_y, (*it)->spu_width, (*it)->spu_height, originDisplayRect, rect, screenRect);
            if ((*it)->subtitle_type != SubtitleType::TYPE_SUBTITLE_DTVKIT_TELETEXT && (*it)->subtitle_type != SubtitleType::TYPE_SUBTITLE_DVB_TELETEXT) {
                std::this_thread::sleep_for(std::chrono::milliseconds(15));
            }
        }
    }

    ALOGD("After draw items: %d", mShowingSubs.size());
}

bool FBRender::isText(std::shared_ptr<AML_SPUVAR> &spu) {
    return spu->isSimpleText
           || spu->subtitle_type == SubtitleType::TYPE_SUBTITLE_CLOSED_CAPTION
           || spu->subtitle_type == SubtitleType::TYPE_SUBTITLE_SCTE27
           || spu->subtitle_type == SubtitleType::TYPE_SUBTITLE_DTVKIT_SCTE27
           || spu->subtitle_type == SubtitleType::TYPE_SUBTITLE_ARIB_B24
           || spu->subtitle_type == SubtitleType::TYPE_SUBTITLE_DTVKIT_ARIB_B24
           || spu->subtitle_type == SubtitleType::TYPE_SUBTITLE_TTML
           || spu->subtitle_type == SubtitleType::TYPE_SUBTITLE_DTVKIT_TTML;
}

void FBRender::onThreadExit() {
    ALOGD("%s", __FUNCTION__ );
    mFBDevice.reset();
}

void FBRender::clearScreen() {
    ALOGD("clearScreen");
    // mFBDevice->clearSurface();
    // mFBDevice->drawColor(0, 0, 0, 0);
    mFBDevice->clearFullFramebufferScreen();
}

void FBRender::sendMessage(const AMLMessage &message, nsecs_t nsecs) {
    if (nsecs <= 0) {
        mLooper->sendMessage(mHandler, message);
    } else {
        mLooper->sendMessageDelayed(nsecs, mHandler, message);
    }
}
