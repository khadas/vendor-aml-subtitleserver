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

#ifndef _DFBRENDER_H
#define _DFBRENDER_H

#include "Render.h"

#include <list>
#include <utils/Looper.h>
#include <utils/StrongPointer.h>
#include <utils/Mutex.h>

using AMLooper = android::sp<android::Looper>;
using AMLMessage = android::Message;

class DFBDevice;

class DFBRender {
public:

    static DFBRender* GetInstance();
    static void Clear();

    void requestExit();

    bool showSubtitleItem(std::shared_ptr<AML_SPUVAR> spu, int type);

    bool hideSubtitleItem(std::shared_ptr<AML_SPUVAR> spu);

    void resetSubtitleItem();

    void removeSubtitleItem(std::shared_ptr<AML_SPUVAR> spu);

private:
    DFBRender();
    virtual ~DFBRender();

    static void threadFunc(void *data);

    void onThreadExit();

    void sendMessage(const AMLMessage &message, nsecs_t nsecs = 0);

    void dfbInit();

    void drawItems();

    void clearScreen();

    bool isText(std::shared_ptr<AML_SPUVAR>& spu);

    enum {
        kWhat_thread_init,
        kWhat_show_subtitle_item,
        kWhat_clear,
    };

    class AMLHandler : public android::MessageHandler {
    public:
        AMLHandler(DFBRender* dfbRender)
                : mWk_DFBRender(dfbRender) {
        };

        virtual void handleMessage(const AMLMessage &message) override;

    private:
        DFBRender* mWk_DFBRender;
    };

    enum Color {
        transparent = 0x00000000,
        black = 0xff0000ff,
    };

private:
    int mParseType;
    bool mRequestExit = false;

    android::Mutex mRenderMutex;

    AMLooper mLooper;
    android::sp<AMLHandler> mHandler;
    std::shared_ptr<DFBDevice> mDFBDevice;
    std::list<std::shared_ptr<AML_SPUVAR>> mShowingSubs;

    static DFBRender* sInstance;
};

/*Holding the global DFBRender obj*/
class DFBRenderWrapper : public Render {
public:
    DFBRenderWrapper()
    :mDFBRender(DFBRender::GetInstance()){}

    ~DFBRenderWrapper(){
        /*Do not delete*/
        mDFBRender = nullptr;
    }

    virtual RenderType getType() { return DIRECT_RENDER; }

    virtual bool showSubtitleItem(std::shared_ptr<AML_SPUVAR> spu, int type) {
        if (mDFBRender)
            mDFBRender->showSubtitleItem(spu, type);

        return true;
    };

    virtual bool hideSubtitleItem(std::shared_ptr<AML_SPUVAR> spu) {
        if (mDFBRender)
            mDFBRender->hideSubtitleItem(spu);
        return true;
    };

    virtual void resetSubtitleItem() {
        if (mDFBRender)
            mDFBRender->resetSubtitleItem();
    };

    virtual void removeSubtitleItem(std::shared_ptr<AML_SPUVAR> spu) {
        if (mDFBRender)
            mDFBRender->removeSubtitleItem(spu);
    };

private:
    DFBRender* mDFBRender = nullptr;
};


#endif //_DFBRENDER_H
