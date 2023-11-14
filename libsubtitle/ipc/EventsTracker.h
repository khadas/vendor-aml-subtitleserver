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

#ifndef YOCTO_EVENTTRACKER_H
#define YOCTO_EVENTTRACKER_H
#include "SubtitleLog.h"
#include <utils/Looper.h>
#include <thread>
#include <functional>
#include <condition_variable>

using OnThreadStart = std::function<void()>;

class EventsTracker {
public:
    enum Event {
        EVENT_INPUT = android::Looper::EVENT_INPUT,
        EVENT_OUTPUT = android::Looper::EVENT_OUTPUT,
        EVENT_ERROR = android::Looper::EVENT_ERROR,
        EVENT_HANGUP = android::Looper::EVENT_HANGUP,
    };

    enum HandleRet {
        RET_REMOVE = 0,
        RET_CONTINUE = 1,
    };

    EventsTracker(android::Looper_callbackFunc callback, const char *name = nullptr, OnThreadStart onThreadStartPtr = nullptr) {
        mExited = false;

        mThread = std::make_shared<std::thread>([&](android::sp<android::Looper> *looper) {
            if (onThreadStartPtr != nullptr) {
                onThreadStartPtr();
            }

            {
                std::lock_guard<std::mutex> _l(_mutex);
                *looper = android::Looper::prepare(0);
                SUBTITLE_LOGI("EventsTracker, Looper created: %p", (*looper).get());
                _cond.notify_all();
            }

            while (!mRequestExit) {
                (*looper)->pollOnce(-1);
            }
            SUBTITLE_LOGI("EventsTracker: %s EXIT!!!", name);

            {
                std::unique_lock<std::mutex> _l(_mutex_exit);
                mExited = true;
                _cond_exit.notify_all();
            }
        }, &mLooper);

        {
            std::unique_lock<std::mutex> _l(_mutex);
            SUBTITLE_LOGI("EventsTracker, Waiting for mLooper create");
            _cond.wait(_l, [&]() -> bool { return mLooper != nullptr; });
            SUBTITLE_LOGI("EventsTracker, Looper created: %p", mLooper.get());
        }

        mCallback = callback;
    }

    ~EventsTracker() {
        mCallback = nullptr;
        requestExit();
    }

    void requestExit() {
        if (mRequestExit || mLooper == nullptr)
            return;

        mRequestExit = true;
        wake();
    }

    void wake() {
        if (mLooper->isPolling()) {
            mLooper->wake();
        }
    }

    int addFd(int fd, int events, void *data) {
        return mLooper->addFd(fd, android::Looper::POLL_CALLBACK, events, mCallback, data);
    }

    int removeFd(int fd) {
        return mLooper->removeFd(fd);
    }

    void join() {
//        if (mThread != nullptr && mThread->joinable()) {
//            mThread->join();
//        }

        if (mThread != nullptr) {
            std::unique_lock<std::mutex> _l(_mutex_exit);

            if (!mExited) {
                SUBTITLE_LOGI("Waiting for exit...");
                _cond_exit.wait(_l);
                SUBTITLE_LOGI("Exit thread...");
                mExited = true;
            }

            if (mThread->joinable()) {
                mThread->join();
                SUBTITLE_LOGI("join thread...");
            }
        }
    }

private:
    android::sp<android::Looper> mLooper = nullptr;
    android::Looper_callbackFunc mCallback = nullptr;

    volatile bool mRequestExit = false;

    std::mutex _mutex;
    std::condition_variable _cond;

    bool mExited = false;
    std::mutex _mutex_exit;
    std::condition_variable _cond_exit;

    std::shared_ptr<std::thread> mThread;
};

#endif //YOCTO_EVENTTRACKER_H
