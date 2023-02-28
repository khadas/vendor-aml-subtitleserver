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

#define LOG_TAG "DFBRender"

#include "DFBRender.h"

#include <thread>
#include <utils/Log.h>

#include "DFBDevice.h"
#include <Parser.h>
#include <CCJsonParser.h>

DFBRender* DFBRender::sInstance = nullptr;

DFBRender *DFBRender::GetInstance() {
    if (sInstance == nullptr) {
        sInstance = new DFBRender();
    }
    return sInstance;
}

void DFBRender::Clear() {
    if (sInstance != nullptr) {
        delete sInstance;
        sInstance = nullptr;
    }
}

DFBRender::DFBRender() {
    ALOGD("DFBRender +++");
    std::thread thread(DFBRender::threadFunc, this);
    thread.detach();
}

DFBRender::~DFBRender() {
    ALOGD("DFBRender ---");
    requestExit();
}

void DFBRender::threadFunc(void *data) {
    ALOGD("DFBRender threadFunc start");
    DFBRender* render = static_cast<DFBRender *>(data);

    if (render->mLooper == nullptr) {
        render->mHandler = new AMLHandler(render);
        render->mLooper = android::Looper::prepare(0);

        render->sendMessage(android::Message(kWhat_thread_init));
    }

    while (!render->mRequestExit) {
        render->mLooper->pollOnce(-1);
    }

    render->onThreadExit();
    ALOGD("DFBRender thread exit !!!");
}

void DFBRender::AMLHandler::handleMessage(const AMLMessage &message) {
    ALOGD("handleMessage: %d", message.what);
    DFBRender* render = mWk_DFBRender;
    if (mWk_DFBRender == nullptr) {
        return;
    }

    switch (message.what) {
        case kWhat_thread_init:
            render->dfbInit();
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

void DFBRender::requestExit() {
    if (mRequestExit)
        return;

    mRequestExit = true;
    if (mLooper->isPolling()) {
        mLooper->wake();
    }
}

void DFBRender::dfbInit() {
    mDFBDevice = std::make_shared<DFBDevice>();
    if (!mDFBDevice->initCheck()) {
        ALOGE("dfbInit, initCheck failed");
        mLooper->removeMessages(mHandler);
        requestExit();
        return;
    }

    sendMessage(AMLMessage(kWhat_clear));
    ALOGD("RenderDevice init OK.");
}

bool DFBRender::showSubtitleItem(std::shared_ptr<AML_SPUVAR> spu, int type) {
    mRenderMutex.lock();
    mShowingSubs.push_back(spu);
    mRenderMutex.unlock();

    mParseType = type;
    sendMessage(AMLMessage(kWhat_show_subtitle_item));
    return true;
}

bool DFBRender::hideSubtitleItem(std::shared_ptr<AML_SPUVAR> spu) {
    mRenderMutex.lock();
    mShowingSubs.remove(spu);
    mRenderMutex.unlock();

    sendMessage(AMLMessage(kWhat_show_subtitle_item));
    return true;
}

void DFBRender::resetSubtitleItem() {
    mRenderMutex.lock();
    mShowingSubs.clear();
    mRenderMutex.unlock();

    sendMessage(AMLMessage(kWhat_clear));
}

void DFBRender::removeSubtitleItem(std::shared_ptr<AML_SPUVAR> spu) {
    mRenderMutex.lock();
    mShowingSubs.remove(spu);
    mRenderMutex.unlock();

    sendMessage(AMLMessage(kWhat_show_subtitle_item));
}

void DFBRender::drawItems() {
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

        DFBRect rect = DFBRect(x, y, x + w, y + h);
        DFBRect originDisplayRect = DFBRect(0, 0, dsp_w, dsp_h);

        DFBRect screenRect = mDFBDevice->screenRect();
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

            DFBDevice::TextParams textParams;
            textParams.content = text;
            textParams.fontFamily = "Liberation Sans";
            textParams.fontSize = 20;
            textParams.textFillColor = Cairo::Colors::White;
            textParams.textLineColor = Cairo::Colors::White;
            textParams.bgPadding = 10;
            textParams.bgColor = Cairo::Colors::GrayTransparent;

            mDFBDevice->drawMultiText((*it)->subtitle_type, textParams,(*it)->pts, (*it)->buffer_size, (*it)->spu_width, (*it)->spu_height, originDisplayRect, rect, screenRect);
        } else {
            ALOGD("Image type");

            // Show full screen for Teletext
            bool showFullScreen =
                    (*it)->subtitle_type == SubtitleType::TYPE_SUBTITLE_DTVKIT_TELETEXT
                    || (*it)->subtitle_type == SubtitleType::TYPE_SUBTITLE_DVB_TELETEXT;

            mDFBDevice->drawImage((*it)->subtitle_type, (*it)->spu_data, (*it)->pts, (*it)->buffer_size, (*it)->spu_width, (*it)->spu_height, showFullScreen ? rect : originDisplayRect, rect, screenRect);
        }
    }

    ALOGD("After draw items: %d", mShowingSubs.size());
}

bool DFBRender::isText(std::shared_ptr<AML_SPUVAR> &spu) {
    return spu->isSimpleText
           || spu->subtitle_type == SubtitleType::TYPE_SUBTITLE_CLOSED_CAPTION
           || spu->subtitle_type == SubtitleType::TYPE_SUBTITLE_SCTE27
           || spu->subtitle_type == SubtitleType::TYPE_SUBTITLE_DTVKIT_SCTE27;
}

void DFBRender::onThreadExit() {
    ALOGD("%s", __FUNCTION__ );
    mDFBDevice.reset();
}

void DFBRender::clearScreen() {
    ALOGD("clearScreen");
    mDFBDevice->drawColor(0, 0, 0, 0);
}

void DFBRender::sendMessage(const AMLMessage &message, nsecs_t nsecs) {
    if (nsecs <= 0) {
        mLooper->sendMessage(mHandler, message);
    } else {
        mLooper->sendMessageDelayed(nsecs, mHandler, message);
    }
}