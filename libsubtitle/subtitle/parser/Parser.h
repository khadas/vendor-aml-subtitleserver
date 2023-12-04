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

#ifndef __SUBTITLE_PARSER_H__
#define __SUBTITLE_PARSER_H__

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <list>
#include <vector>
#include <condition_variable>
#include <thread>
#include "DataSource.h"
#include "sub_types.h"
#include "sub_types2.h"   //for cc scte27
#ifdef ANDROID
#include "cutils/properties.h"
#endif
#include "ParserEventNotifier.h"
#include <sys/types.h>
#include "VideoInfo.h"


#ifndef LOGD
#define LOGD SUBTITLE_LOGI
#endif
#ifndef LOGE
#define LOGE SUBTITLE_LOGE
#endif
#ifndef LOGI
#define LOGI SUBTITLE_LOGI
#endif
#ifndef LOGV
#define LOGV SUBTITLE_LOGI
#endif

//before is disorder with amnuplayer send,now
//sync with amnuplayer client send definition
enum SubtitleType {
    TYPE_SUBTITLE_INVALID   = -1,
    TYPE_SUBTITLE_VOB       = 0,   //dvd subtitle
    TYPE_SUBTITLE_PGS,
    TYPE_SUBTITLE_MKV_STR,
    TYPE_SUBTITLE_SSA,
    TYPE_SUBTITLE_MKV_VOB,
    TYPE_SUBTITLE_DVB,
    TYPE_SUBTITLE_TMD_TXT   = 7,
    TYPE_SUBTITLE_IDX_SUB,  //now discard
    TYPE_SUBTITLE_DVB_TELETEXT,
    TYPE_SUBTITLE_CLOSED_CAPTION,
    TYPE_SUBTITLE_SCTE27,
    TYPE_SUBTITLE_DTVKIT_DVB, //12
    TYPE_SUBTITLE_DTVKIT_TELETEXT,
    TYPE_SUBTITLE_DTVKIT_SCTE27,
    TYPE_SUBTITLE_EXTERNAL, //15
    TYPE_SUBTITLE_ARIB_B24,
    TYPE_SUBTITLE_DTVKIT_ARIB_B24,
    TYPE_SUBTITLE_TTML,
    TYPE_SUBTITLE_DTVKIT_TTML,
    TYPE_SUBTITLE_SMPTE_TTML, //20
    TYPE_SUBTITLE_DTVKIT_SMPTE_TTML,
    TYPE_SUBTITLE_MAX,
};


#define SUB_INIT        0
#define SUB_PLAYING     1
#define SUB_STOP        2
#define SUB_EXIT        3
#define HEADER_SIZE 20

#define VOB_SUB_WIDTH 1920
#define VOB_SUB_HEIGHT 1280
#define VOB_SUB_SIZE VOB_SUB_WIDTH*VOB_SUB_HEIGHT/4
#define DVB_SUB_SIZE VOB_SUB_WIDTH*VOB_SUB_HEIGHT*4
#define SCTE27_SUB_SIZE 1920*1080*4

#define INTERNAL_MAX_NUMBER_SPU_ITEM 20
#define EXTERNAL_MAX_NUMBER_SPU_ITEM 500
#define FILTER_DATA_TIMEROUT 10*1000


static const int AML_PARSER_SYNC_WORD = 'AMLU';

class Parser {
public:
    Parser() {
        mParseType = -1;
        mNotifier = nullptr;
        mDataNotifier = nullptr;
        mThreadExitRequested = false;
        mState = -1;
        mMaxSpuItems = INTERNAL_MAX_NUMBER_SPU_ITEM;
    }
    virtual ~Parser() {
        mState = SUB_EXIT;
        mPtsRecord = 0;
    };

    virtual int parse(/*params*/) = 0;

    /* default impl not used. for some parser to override */
    virtual bool updateParameter(int type, void *data) {
        (void) type;
        (void) data;
        return false;
    }

    /* default impl not used. for some parser to override */
    virtual void setPipId (int mode, int id) {
        (void)mode;
        (void)id;
        return;
     }

   virtual void resetForSeek() {
        std::unique_lock<std::mutex> autolock(mMutex);
        mDecodedSpu.clear();
    }

    bool startParser(ParserEventNotifier *notify, ParserSubdataNotifier *dataNofity) {
        mState = SUB_INIT;
        mNotifier = notify;
        mDataNotifier = dataNofity;
        mThread = std::thread(&Parser::_parserEntry, this);
        return true;
    }

    int getParseType(){
        return mParseType;
    }

    bool stopParser() {
        { // narrow the lock scope
            std::unique_lock<std::mutex> autolock(mMutex);
            mDecodedSpu.clear();
            mState = SUB_STOP;
            if (mThreadExitRequested)
                return false;

            mThreadExitRequested = true;
        }
        mThread.join();
        return true;
    }

    // TODO: split to a spu manager class
    std::shared_ptr<AML_SPUVAR> consumeDecodedItem() {
        std::shared_ptr<AML_SPUVAR> item;
        std::unique_lock<std::mutex> autolock(mMutex);
        while (mDecodedSpu.size() == 0) {
            mCv.wait(autolock);
        }
        item = mDecodedSpu.front();
        mDecodedSpu.pop_front();
        return item;
    }

   virtual std::shared_ptr<AML_SPUVAR> tryConsumeDecodedItem() {
        std::shared_ptr<AML_SPUVAR> item;
        std::unique_lock<std::mutex> autolock(mMutex);
        if (mDecodedSpu.size() == 0) {
            //__android_log_print(ANDROID_LOG_ERROR, "Parser", "in parser item->spu_data null");
            return nullptr;
        }
        item = mDecodedSpu.front();
        mDecodedSpu.pop_front();
        return item;
    }

    virtual void addDecodedItem(std::shared_ptr<AML_SPUVAR> item) {
        std::unique_lock<std::mutex> autolock(mMutex);

        // Do not add when exited, it's useless.
        if (mState == SUB_STOP || mState == SUB_EXIT) {
            return;
        }
        while (mDecodedSpu.size() >= mMaxSpuItems) {
            mDecodedSpu.pop_front();
        }

        // validate the item: do not sent empty items due to parser error.
        if (item == nullptr || item->spu_data == nullptr) {
#ifdef ANDROID
            __android_log_print(ANDROID_LOG_ERROR, "Parser", "add invalid empty spu!");
#endif
            return;
        }

        mDecodedSpu.push_back(item);
        if (mDataNotifier != nullptr) mDataNotifier->notifySubdataAdded();
        mCv.notify_all();
    }

    bool isExternalSub() {
        return mParseType == TYPE_SUBTITLE_EXTERNAL;
    }
    virtual void dump(int fd, const char *prefix) = 0;
    std::shared_ptr<DataSource> mDataSource;


protected:
    ParserEventNotifier *mNotifier;
    // TODO: included in  spu manager class
    ParserSubdataNotifier *mDataNotifier;
    bool mThreadExitRequested;
    int mState;
    int mParseType;
    int mMaxSpuItems;
    int64_t mPtsRecord = 0;

    void dumpCommon(int fd, const char *prefix) {
        dprintf(fd, "%s DataSource=%p\n", prefix, mDataSource.get());
        dprintf(fd, "%s ExitRequested ? %s\n", prefix, mThreadExitRequested?"yes":"no");
        dprintf(fd, "%s State=%d\n", prefix, mState);
        dprintf(fd, "%s Parser Type=%d\n", prefix, mParseType);
        dprintf(fd, "%s Max allowed SPUs (%d)\n", prefix, mMaxSpuItems);
    }
    std::list<std::shared_ptr<AML_SPUVAR>> mDecodedSpu;
    std::thread mThread;

private:

    std::mutex mMutex;
    std::condition_variable mCv;

    void _parserEntry() {
        while (!mThreadExitRequested && mState != SUB_STOP && mState != SUB_EXIT) {
            parse();
        }
    }

};

#endif
