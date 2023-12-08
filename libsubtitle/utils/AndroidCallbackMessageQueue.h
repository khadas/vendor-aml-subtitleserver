#pragma once

#include <list>
#include <thread>
#include <utils/Mutex.h>
#include <utils/CallStack.h>

#include "ParserEventNotifier.h"

//#include <android/hidl/allocator/1.0/IAllocator.h>
#include <binder/IMemory.h>
//#include <hidlmemory/mapping.h>
//SubtitleType.h
//#include <vendor/amlogic/hardware/subtitleserver/1.0/types.h>
#include <utils/Looper.h>
#include <SubtitleType.h>
using namespace android;

//using ::android::CallStack;
//using ::android::hardware::HidlMemory;
//using ::android::hardware::hidl_memory;
//using ::android::hardware::mapMemory;
//using ::android::hardware::Return;
//using ::android::hidl::memory::V1_0::IMemory;
//using ::android::hidl::allocator::V1_0::IAllocator;

//using ::vendor::amlogic::hardware::subtitleserver::V1_0::SubtitleHidlParcel;

namespace android {

class AndroidCallbackHandler : public android::RefBase {
public:
    virtual ~AndroidCallbackHandler() {};
    virtual bool onSubtitleDisplayNotify(SubtitleHidlParcel &event) = 0;

    virtual bool onSubtitleEventNotify(SubtitleHidlParcel &event) = 0;

};

class AndroidCallbackMessageQueue : public ParserEventNotifier, public android::MessageHandler {
public:
    static sp<AndroidCallbackMessageQueue> Instance();

    AndroidCallbackMessageQueue() = delete;
    AndroidCallbackMessageQueue(sp<AndroidCallbackHandler> proxy);
    ~AndroidCallbackMessageQueue();

    bool postDisplayData(const char *data,  int type,
            int x, int y, int width, int height,
            int videoWidth, int videoHeight, int size, int cmd);

    virtual void onSubtitleDataEvent(int event, int id) override;

    virtual void onSubtitleDimension(int width, int height) override;
    // report available state, may error
    virtual void onSubtitleAvailable(int available) override;

    virtual void onVideoAfdChange(int afd, int playerid) override;

    //for ttx page pip
    virtual void onMixVideoEvent(int val) override;

    virtual void onSubtitleLanguage(char* lang) override;

    //what: info type, extra: value, for info extension
    virtual void onSubtitleInfo(int what, int extra) override;


private:
    struct SubtitleData {
        IMemory* mem;
        int type;
        int x;
        int y;
        int width;
        int height;
        int videoWidth;
        int videoHeight;
        int size;
        int isShow;
        int shmid;
        char *lang;
    };

    void handleMessage(const android::Message& message);
    void looperLoop();
     int copyShareMemory(const char* data, int size);
    std::thread mCallbackLooperThread;
    sp<android::Looper> mLooper;
    std::list<std::unique_ptr<SubtitleData>> mSubtitleData;


    // we transfer to proxy to do the real work.
    sp<AndroidCallbackHandler> mProxyHandler;

    mutable android::Mutex mLock;
    bool mStopped;
    static sp<AndroidCallbackMessageQueue> sInstance;
};

}
