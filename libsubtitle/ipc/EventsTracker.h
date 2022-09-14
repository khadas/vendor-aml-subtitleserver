//
// Created by jiajia.tian on 2021/11/15.
//

#ifndef YOCTO_EVENTTRACKER_H
#define YOCTO_EVENTTRACKER_H

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
                ALOGD("EventsTracker, Looper created: %p", (*looper).get());
                _cond.notify_all();
            }

            while (!mRequestExit) {
                (*looper)->pollOnce(-1);
            }
            ALOGD("EventsTracker: %s EXIT!!!", name);

            {
                std::unique_lock<std::mutex> _l(_mutex_exit);
                mExited = true;
                _cond_exit.notify_all();
            }
        }, &mLooper);

        {
            std::unique_lock<std::mutex> _l(_mutex);
            ALOGD("EventsTracker, Waiting for mLooper create");
            _cond.wait(_l, [&]() -> bool { return mLooper != nullptr; });
            ALOGD("EventsTracker, Looper created: %p", mLooper.get());
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
                ALOGD("Waiting for exit...");
                _cond_exit.wait(_l);
                ALOGD("Exit thread...");
                mExited = true;
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
