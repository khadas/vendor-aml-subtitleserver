#pragma once

#include <thread>
#include <mutex>
// maybe we can use std::chrono::steady_clock
#include <utils/Timers.h>
//#include "AmlLooper.h"
 #include <utils/Looper.h>
#include <binder/Binder.h>
#include "Parser.h"

#include "Render.h"
#include "Display.h"

using android::sp;
using android::Looper;
using android::Message;

class Presentation  : public ParserSubdataNotifier{

public:
    Presentation(std::shared_ptr<Display> disp, int renderType);
    virtual ~Presentation();

    bool syncCurrentPresentTime(int64_t pts);
    bool notifyStartTimeStamp(int64_t startTime);

    bool startPresent(std::shared_ptr<Parser> parser);
    bool stopPresent();
    bool resetForSeek();
    bool combineSamePtsSubtitle(std::shared_ptr<AML_SPUVAR> spu1, std::shared_ptr<AML_SPUVAR> spu2);
    bool compareBitAndSyncPts(std::shared_ptr<AML_SPUVAR> spu, int64_t pts);


    virtual void notifySubdataAdded();

    void dump(int fd, const char *prefix);

private:
    class MessageProcess : public android::MessageHandler {
    public:
        static const int MSG_PTS_TIME_UPDATED     = 0xBB10100;
        static const int MSG_PTS_TIME_CHECK_SPU   = 0xBB10101;
        static const int MSG_RESET_MESSAGE_QUEUE  = 0xCB10100;

        MessageProcess(Presentation *present, bool isExtSub);
        virtual ~MessageProcess();
        void looperLoop();
        void join();
        bool notifyMessage(int what);

    private:
        std::shared_ptr<std::thread> mLooperThread;
        std::mutex mLock;
        std::condition_variable mCv;
        Presentation *mPresent; // this use shared_ptr, decouple with sp raw pointer here
        bool mIsExtSub;
        bool mRequestThreadExit;
        void handleMessage(const Message& message);
        void handleExtSub(const Message& message);
        void handleStreamSub(const Message& message);
        std::shared_ptr<AML_SPUVAR> mLastShowingSpu;
        //std::shared_ptr<Looper> mLooper;
                sp<Looper> mLooper;

    };

    int64_t mCurrentPresentRelativeTime;
    int64_t mStartPresentMonoTimeNs;
    int64_t mStartTimeModifier;

    std::shared_ptr<Parser> mParser;
    std::shared_ptr<Display> mDisplay;
    std::shared_ptr<Render> mRender;

    // for present subtitle.
    std::list<std::shared_ptr<AML_SPUVAR>> mEmittedShowingSpu;
    std::list<std::shared_ptr<AML_SPUVAR>> mEmittedFaddingSpu;

    /* this use android sp, so de-couple with shared_ptr */
    sp<MessageProcess> mMsgProcess;

    // since mMsgProcess is a pointer here
    // we need a mutext to protect it
    std::mutex mMutex;


};



