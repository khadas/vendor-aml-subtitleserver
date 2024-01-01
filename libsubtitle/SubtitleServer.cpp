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

#define LOG_TAG "SubtitleServer"
//#include <fmq/EventFlag.h>
#include "SubtitleServer.h"
#include "MemoryLeakTrackUtil.h"
#include "SubtitleLog.h"
#include <utils/CallStack.h>
#include <SubtitleSignalHandler.h>

using android::CallStack;

SubtitleServer *SubtitleServer::sInstance = nullptr;

SubtitleServer *SubtitleServer::Instance() {
    if (!sInstance) sInstance = new SubtitleServer();
    return sInstance;
}

const static int FIRST_SESSION_ID = 0x1F230000;
const static int LAST_SESSION_ID = 0x7FFFFFFF;
static pthread_once_t threadFlag = PTHREAD_ONCE_INIT;

SubtitleServer::SubtitleServer() {
    pthread_once(&threadFlag, []{
        SubtitleSignalHandler::instance().installSignalHandlers();
    });

    // mDeathRecipient =new DeathRecipient(this);
    // mFallbackPlayStarted = false;
    //  mFallbackCallback = nullptr;
    // Here, we create this messageQueue, it automatically start a thread to pump
    // decoded data/event from parser, and call CallbackHandler to send to remote side.
    mMessageQueue = new AndroidCallbackMessageQueue(new ClientMessageHandlerImpl(this));

    mCurSessionId = FIRST_SESSION_ID;
    mOpenCalled = false;
    mLastOpenType = OpenType::TYPE_APPSDK;
    mLastOpenTime = -1;
}


std::shared_ptr<SubtitleService> SubtitleServer::getSubtitleServiceLocked(int sId) {
    SUBTITLE_LOGI("getSubtitleServiceLocked, sId= %d", sId);

    auto search = mServiceClients.find((sId));
    if (search != mServiceClients.end()) {
        return search->second;
    } else {
        SUBTITLE_LOGE("Error! cannot found service by id:%x", sId);
    }

    return nullptr;
}

std::shared_ptr<SubtitleService> SubtitleServer::getSubtitleService(int sId) {
    android::AutoMutex _l(mLock);
    return getSubtitleServiceLocked(sId);
}

// Methods from ISubtitleServer follow.
//void SubtitleServer::openConnection(openConnection_cb _hidl_cb) {
int SubtitleServer::openConnection() {
    SUBTITLE_LOGI("%s ", __func__);
    int sessionId = -1;
    {
        android::AutoMutex _l(mLock);
        sessionId = mCurSessionId++;
        if (sessionId == LAST_SESSION_ID) {
            sessionId = mCurSessionId = LAST_SESSION_ID;
        }

        sessionId = 0;
        if (mServiceClients.find(sessionId) == mServiceClients.end()) {
            //closeConnection called
            SUBTITLE_LOGI("create SubtitleService for sessionId %d", sessionId);
            auto ss = std::make_shared<SubtitleService>();
            //auto p = std::make_pair(sessionId, ss);
            //mClients.insert(p);
            mServiceClients[sessionId] = ss;
        } else {
            SUBTITLE_LOGI("SubtitleService for sessionId %d already ok.", 0);
        }

        SUBTITLE_LOGI("openConnection: size:%d", mServiceClients.size());
        for (int i = 0; i < mServiceClients.size(); i++) {
            SUBTITLE_LOGI("client %d-%d: %p", i, mServiceClients.size(), mServiceClients[i].get());
        }

    }
    return sessionId;
    //_hidl_cb(Result::OK, sessionId);
    //return Void();
}

Result SubtitleServer::closeConnection(int32_t sId) {
    SUBTITLE_LOGI("%s sId=%d", __func__, sId);

    //TODO: too simple here! need more condition.
    //hide(sId); // for standalone play, need hide!
    SUBTITLE_LOGI("need hide here");


    android::AutoMutex _l(mLock);

    int clientSize = mServiceClients.size();
    SUBTITLE_LOGI("clientSize=%d", clientSize);
#if SUPPORT_MULTI
    for (auto it = mServiceClients.begin(); it != mServiceClients.end();) {
        if (it->first == sId) {
            mServiceClients.erase(it);
            return Result::OK;
        }
        it++;
    }
#else
    if (!mServiceClients.empty()) {
        for (auto it = mServiceClients.begin(); it != mServiceClients.end();) {
            if (it->first == sId) {
                SUBTITLE_LOGI("closeConnection for sessionId %d", sId);
                mServiceClients.erase(it);
                break;
            } else {
                it++;
            }
        }
    }
#endif
    return Result::FAIL;
}

Result SubtitleServer::open(int32_t sId, int32_t ioType, OpenType openType,
                            const native_handle_t *handle) {
    android::AutoMutex _l(mLock);
    std::shared_ptr<SubtitleService> ss = getSubtitleServiceLocked(sId);
    SUBTITLE_LOGI("%s ss=%p ioType=%d openType:%d", __func__, ss.get(), ioType, openType);

    std::vector<int> fds;
    int idxSubId = -1;
    //int dupFd = -1; // fd will auto closed when destruct hidl_handle, dump one.
    int demuxId = -1;
    if (handle != nullptr && handle->numFds >= 1) {
        for (int i=0; i<handle->numFds; i++) {
            int fd = handle->data[i];
            fds.push_back(::dup(fd));
        }

        if (handle->numInts > 0) {
            idxSubId = handle->data[handle->numFds];
        }
    }

    auto now = systemTime(SYSTEM_TIME_MONOTONIC);
    const int64_t diff200ms = 200000000LL;
    SUBTITLE_LOGI("mOpenCalled: %d mLastOpenType=%d, mLastOpenTime=%lld, now=%lld",
          mOpenCalled, mLastOpenType, mLastOpenTime, now);

    if (ss != nullptr) {
        // because current we test middleware player(e.g CTC) with VideoPlayer, it know nothing about
        // middleware calling, if ctc called, close VideoPlayer's request, use CTC's request.
        if (mOpenCalled && openType == OpenType::TYPE_MIDDLEWARE) {
            if ((openType != mLastOpenType && (now - mLastOpenTime) < diff200ms) ||
                mLastOpenTime == -1) {
                bool r = ss->stopSubtitle();
                /* ss->stopFmqReceiver();
                 if (mDataMQ) {
                     std::unique_ptr<DataMQ> removeDataMQ(std::move(mDataMQ));
                     mDataMQ.reset(nullptr);
                 }*/
            }
        }
        // TODO: need revise, fix the value.
        if ((ioType&0xff) == E_SUBTITLE_DEMUX) {
            demuxId = ioType >> 16;
            ioType = ioType & 0xff;
            SUBTITLE_LOGI("mOpenCalled : demux id= %d, ioType =%d\n", demuxId, ioType);
            ss->setDemuxId(demuxId);
        }
        bool r = ss->startSubtitle(fds, idxSubId, (SubtitleIOType)ioType, mMessageQueue.get());

        mOpenCalled = true;
        mLastOpenType = openType;
        mLastOpenTime = now;

        return (r ? Result::OK : Result::FAIL);
    }

    //if (dupFd != -1) close(dupFd);
    SUBTITLE_LOGI("no valid ss, Should not enter here!");
    return Result::FAIL;
}

Result SubtitleServer::close(int32_t sId) {
    std::shared_ptr<SubtitleService> ss = getSubtitleService(sId);
    SUBTITLE_LOGI("%s ss=%p", __func__, ss.get());
    if (ss != nullptr) {
        bool r = ss->stopSubtitle();
        /*  ss->stopFmqReceiver();
          {
              android::AutoMutex _l(mLock);
              if (mDataMQ) {
                  std::unique_ptr<DataMQ> removeDataMQ(std::move(mDataMQ));
                  mDataMQ.reset(nullptr);
              }
              mOpenCalled = false;
          }*/
        return (r ? Result::OK : Result::FAIL);
    }

    return Result::FAIL;
}

Result SubtitleServer::resetForSeek(int32_t sId) {
    std::shared_ptr<SubtitleService> ss = getSubtitleService(sId);
    if (ss != nullptr) {
        bool r = ss->resetForSeek();
        return (r ? Result::OK : Result::FAIL);
    }
    return Result::FAIL;
}

Result SubtitleServer::updateVideoPos(int32_t sId, int64_t pos) {
    std::shared_ptr<SubtitleService> ss = getSubtitleService(sId);
    if (ss != nullptr) {
        bool r = ss->updateVideoPosAt(pos);
        return (r ? Result::OK : Result::FAIL);
    }
    return Result::FAIL;
}

//void SubtitleServer::getTotalTracks(int32_t sId, getTotalTracks_cb _hidl_cb) {
int SubtitleServer::getTotalTracks(int32_t sId) {
    std::shared_ptr<SubtitleService> ss = getSubtitleService(sId);
    SUBTITLE_LOGI("%s ss=%p", __func__, ss.get());
    if (ss != nullptr) {
        SUBTITLE_LOGI("%s total=%d", __func__, ss->totalSubtitles());
        //_hidl_cb(Result::OK, ss->totalSubtitles());
        return ss->totalSubtitles();
    } else {
        // _hidl_cb(Result::FAIL, 1);//-1);
        return -1;
    }

    // return Void();
}

//void SubtitleServer::getType(int32_t sId, getType_cb _hidl_cb) {
int SubtitleServer::getType(int32_t sId) {
    std::shared_ptr<SubtitleService> ss = getSubtitleService(sId);
    SUBTITLE_LOGI("%s ss=%p", __func__, ss.get());
    if (ss != nullptr) {
        SUBTITLE_LOGI("%s subType=%d", __func__, ss->subtitleType());
        //_hidl_cb(Result::OK, ss->subtitleType());
        return ss->subtitleType();
    } else {
        //_hidl_cb(Result::FAIL, 1);
        return -1;
    }
    //return Void();
}

//void SubtitleServer::getLanguage(int32_t sId, getLanguage_cb _hidl_cb) {
std::string SubtitleServer::getLanguage(int32_t sId) {
    std::shared_ptr<SubtitleService> ss = getSubtitleService(sId);
    SUBTITLE_LOGI("%s ss=%p", __func__, ss.get());
    if (ss != nullptr) {
        return ss->currentLanguage();
    } else {
        return std::string("No lang");
    }
}

Result SubtitleServer::setSubType(int32_t sId, int32_t type) {
    std::shared_ptr<SubtitleService> ss = getSubtitleService(sId);
    SUBTITLE_LOGI("%s ss=%p subType=%d", __func__, ss.get(), type);
    if (ss != nullptr) {
        ss->setSubType(type);
    }
    return Result {};
}

Result SubtitleServer::setSubPid(int32_t sId, int32_t pid) {
    std::shared_ptr<SubtitleService> ss = getSubtitleService(sId);
    SUBTITLE_LOGI("%s ss=%p pid=%d", __func__, ss.get(), pid);
    if (ss != nullptr) {
        ss->setSubPid(pid);
    }
    return Result {};
}

Result SubtitleServer::setPageId(int32_t sId, int32_t pageId) {
    std::shared_ptr<SubtitleService> ss = getSubtitleService(sId);
    SUBTITLE_LOGI("%s ss=%p", __func__, ss.get());
    if (ss != nullptr) {
        ss->setSubPageId(pageId);
    }
    return Result {};
}

Result SubtitleServer::setAncPageId(int32_t sId, int32_t ancPageId) {
    std::shared_ptr<SubtitleService> ss = getSubtitleService(sId);
    SUBTITLE_LOGI("%s ss=%p setAncPageId=%d", __func__, ss.get(), ancPageId);
    if (ss != nullptr) {
        ss->setSubAncPageId(ancPageId);
    }
    return Result {};
}

Result SubtitleServer::setSecureLevel(int32_t sId, int32_t flag) {
    std::shared_ptr<SubtitleService>  ss = getSubtitleService(flag);
    SUBTITLE_LOGI("%s ss=%p", __func__, ss.get());
    if (ss != nullptr) {
        ss->setSecureLevel(flag);
    }
    return Result {};
}

Result SubtitleServer::setChannelId(int32_t sId, int32_t channelId) {
    std::shared_ptr<SubtitleService>  ss = getSubtitleService(sId);
    SUBTITLE_LOGI("%s ss=%p", __func__, ss.get());
    if (ss != nullptr) {
        ss->setChannelId(channelId);
    }
    return Result {};
}

Result SubtitleServer::setClosedCaptionVfmt(int32_t sId, int32_t vfmt) {
    std::shared_ptr<SubtitleService> ss = getSubtitleService(sId);
    SUBTITLE_LOGI("%s ss=%p", __func__, ss.get());
    if (ss != nullptr) {
        ss->setClosedCaptionVfmt(vfmt);
    }
    return Result{};
}

Result SubtitleServer::ttControl(int32_t sId, int cmd, int magazine, int pageNo, int regionId, int param) {
    std::shared_ptr<SubtitleService> ss = getSubtitleService(sId);
    if (ss == nullptr) {
        return Result::FAIL;
    }
    SUBTITLE_LOGI("%s ss=%p cmd=%d", __func__, ss.get(), cmd);

    bool r = ss->ttControl(cmd, magazine, pageNo, regionId, param);
    return r ? Result::OK : Result::FAIL;
}

Result SubtitleServer::ttGoHome(int32_t sId) {
    std::shared_ptr<SubtitleService> ss = getSubtitleService(sId);
    if (ss == nullptr) {
        return Result::FAIL;
    }
    ss->ttGoHome();
    return Result::OK;
}

Result SubtitleServer::ttNextPage(int32_t sId, int32_t dir) {
    std::shared_ptr<SubtitleService> ss = getSubtitleService(sId);
    if (ss == nullptr) {
        return Result::FAIL;
    }
    ss->ttNextPage(dir);
    return Result::OK;
}

Result SubtitleServer::ttNextSubPage(int32_t sId, int32_t dir) {
    std::shared_ptr<SubtitleService> ss = getSubtitleService(sId);
    if (ss == nullptr) {
        return Result::FAIL;
    }
    ss->ttNextSubPage(dir);
    return Result::OK;
}

Result SubtitleServer::ttGotoPage(int32_t sId, int32_t pageNo, int32_t subPageNo) {
    std::shared_ptr<SubtitleService> ss = getSubtitleService(sId);
    if (ss == nullptr) {
        return Result::FAIL;
    }
    SUBTITLE_LOGE(" ttGotoPage pageNo=%d, subPageNo=%d", pageNo, subPageNo);
    ss->ttGotoPage(pageNo, subPageNo);
    return Result::OK;
}

Result SubtitleServer::setPipId(int32_t sId, int32_t mode, int32_t id) {
    std::shared_ptr<SubtitleService> ss = getSubtitleService(sId);
    if (ss == nullptr) {
        return Result::FAIL;
    }
    if ((PIP_PLAYER_ID != mode) && (PIP_MEDIASYNC_ID != mode)) {
        return Result::FAIL;
    }
    SUBTITLE_LOGI(" setPipId mode=%d, id=%d", mode, id);
    ss->setPipId(mode, id);
    return Result::OK;
}

Result SubtitleServer::userDataOpen(int32_t sId) {
    SUBTITLE_LOGI("%s", __func__);
    std::shared_ptr<SubtitleService>  ss = getSubtitleService(sId);
    if (ss == nullptr) {
        return Result::FAIL;
    }
    ss->userDataOpen(mMessageQueue.get());
    return Result::OK;
}

Result SubtitleServer::userDataClose(int32_t sId) {
    SUBTITLE_LOGI("%s", __func__);
    std::shared_ptr<SubtitleService>  ss = getSubtitleService(sId);
    if (ss == nullptr) {
        return Result::FAIL;
    }
    ss->userDataClose();
    return Result::OK;
}


#if 0
void SubtitleServer::prepareWritingQueue(int32_t sId, int32_t size, prepareWritingQueue_cb _hidl_cb) {
    auto sendError = [&_hidl_cb](Result result) {
        _hidl_cb(result, DataMQ::Descriptor());
    };

    SUBTITLE_LOGI("%s", __func__);
    android::AutoMutex _l(mLock);

    // Create message queues.
    if (mDataMQ) {
        SUBTITLE_LOGE("the client attempts to call prepareForWriting twice");
    } else {
        std::unique_ptr<DataMQ> tempDataMQ(new DataMQ(size, true /* EventFlag */));
        if (!tempDataMQ->isValid()) {
            SUBTITLE_LOGE(!tempDataMQ->isValid(), "data MQ is invalid");
            sendError(Result::FAIL);
            return Void();
        }

        mDataMQ = std::move(tempDataMQ);
    }

    //TODO: create new thread for fetch the client wrote data.
    std::unique_ptr<FmqReader> tempReader(new FmqReaderImpl(mDataMQ.get()));

    std::shared_ptr<SubtitleService>  ss = getSubtitleServiceLocked(sId);
    SUBTITLE_LOGI("%s ss=%p", __func__, ss.get());
    if (ss != nullptr) {
        ss->startFmqReceiver(std::move(tempReader));
    } else {
        SUBTITLE_LOGE("Error! cannot get subtitle service through ID:%d(%x)", sId, sId);
    }

    _hidl_cb(Result::OK, *mDataMQ->getDesc());

    return Void();
}

#endif

void SubtitleServer::setCallback(const sp<ISubtitleCallback> &callback, ConnectType type) {
//void SubtitleServer::setCallback( sp<ISubtitleCallback>& callback, ConnectType type) {
    android::AutoMutex _l(mLock);
    if (callback != nullptr) {
        int cookie = -1;
        int clientSize = mCallbackClients.size();
        for (int i = 0; i < clientSize; i++) {
            if (mCallbackClients[i] == nullptr) {
                SUBTITLE_LOGI("%s, client index:%d had died, this id give the new client", __FUNCTION__, i);
                cookie = i;
                mCallbackClients[i] = callback;
                break;
            }
        }

        if (cookie < 0) {
            cookie = clientSize;
            mCallbackClients[clientSize] = callback;
        }

        /* Return<bool> linkResult = callback->linkToDeath(mDeathRecipient, cookie);
         bool linkSuccess = linkResult.isOk() ? static_cast<bool>(linkResult) : false;
         if (!linkSuccess) {
             SUBTITLE_LOGE("Couldn't link death recipient for cookie: %d", cookie);
         }
     */
        SUBTITLE_LOGI("%s cookie:%d, client size:%d", __FUNCTION__, cookie, (int) mCallbackClients.size());
    }

    return;
}

void SubtitleServer::setFallbackCallback(const sp<ISubtitleCallback> &callback, ConnectType type) {
    android::AutoMutex _l(mLock);
    mFallbackCallback = callback;
    return;
}

void SubtitleServer::removeCallback(const sp<ISubtitleCallback> &callback) {
    android::AutoMutex _l(mLock);
    if (callback != nullptr) {
        // Remove, if fallback callback.
        /*    if (mFallbackCallback != nullptr && mFallbackCallback.get() == callback.get()) {
                mFallbackCallback->unlinkToDeath(mDeathRecipient);
                mFallbackCallback = nullptr;
            }
    */
        int clientSize = mCallbackClients.size();
        SUBTITLE_LOGI("[removeCallback] remove:%p clientSize=%d", callback.get(), clientSize);
        for (auto it = mCallbackClients.begin(); it != mCallbackClients.end();) {
            /*if (it->second != nullptr) {
                SUBTITLE_LOGI("[removeCallback] %p", (it->second).get());
                it->second->unlinkToDeath(mDeathRecipient);
            }*/
            it = mCallbackClients.erase(it);
        }
    }
    return;
}

#if 0
// This only valid for global fallback display.
Result SubtitleServer::show(int32_t sId) {
    SubtitleHidlParcel parcel;
    android::AutoMutex _l(mLock);
    mFallbackPlayStarted = true;
    parcel.msgType = (int)FallthroughUiCmd::CMD_UI_SHOW;
    parcel.bodyInt.resize(0);
    sendUiEvent(parcel);

    // Can send display Data to subtitle service now.
    return Result {};
}

Result SubtitleServer::hide(int32_t sId) {
    SubtitleHidlParcel parcel;
    android::AutoMutex _l(mLock);
    mFallbackPlayStarted = false;
    parcel.msgType = (int)FallthroughUiCmd::CMD_UI_HIDE;
    parcel.bodyInt.resize(0);
    sendUiEvent(parcel);
    return Result {};
}

/*CMD_UI_SHOW = 0,
CMD_UI_SET_IMGRATIO,
CMD_UI_SET_SUBTITLE_DIMENSION,
CMD_UI_SET_SURFACERECT*/

Result SubtitleServer::setTextColor(int32_t sId, int32_t color) {
    SubtitleHidlParcel parcel;
    android::AutoMutex _l(mLock);
    parcel.msgType = (int)FallthroughUiCmd::CMD_UI_SET_TEXTCOLOR;
    parcel.bodyInt.resize(1);
    parcel.bodyInt[0] = color;
    sendUiEvent(parcel);
    return Result {};
}

Result SubtitleServer::setTextSize(int32_t sId, int32_t size) {
    SubtitleHidlParcel parcel;
    android::AutoMutex _l(mLock);
    parcel.msgType = (int)FallthroughUiCmd::CMD_UI_SET_TEXTSIZE;
    parcel.bodyInt.resize(1);
    parcel.bodyInt[0] = size;
    sendUiEvent(parcel);
    return Result {};
}

Result SubtitleServer::setGravity(int32_t sId, int32_t gravity) {
    SubtitleHidlParcel parcel;
    android::AutoMutex _l(mLock);
    parcel.msgType = (int)FallthroughUiCmd::CMD_UI_SET_GRAVITY;
    parcel.bodyInt.resize(1);
    parcel.bodyInt[0] = gravity;
    sendUiEvent(parcel);
    return Result {};
}

Result SubtitleServer::setTextStyle(int32_t sId, int32_t style) {
    SubtitleHidlParcel parcel;
    android::AutoMutex _l(mLock);
    parcel.msgType = (int)FallthroughUiCmd::CMD_UI_SET_TEXTSTYLE;
    parcel.bodyInt.resize(1);
    parcel.bodyInt[0] = style;
    sendUiEvent(parcel);
    return Result {};
}

Result SubtitleServer::setPosHeight(int32_t sId, int32_t yOffset) {
    SubtitleHidlParcel parcel;
    android::AutoMutex _l(mLock);
    parcel.msgType = (int)FallthroughUiCmd::CMD_UI_SET_POSHEIGHT;
    parcel.bodyInt.resize(1);
    parcel.bodyInt[0] = yOffset;
    sendUiEvent(parcel);
    return Result {};
}

Result SubtitleServer::setImgRatio(int32_t sId, float ratioW, float ratioH, int32_t maxW, int32_t maxH) {
    SubtitleHidlParcel parcel;
    android::AutoMutex _l(mLock);
    parcel.msgType = (int)FallthroughUiCmd::CMD_UI_SET_IMGRATIO;
    parcel.bodyInt.resize(4);
    parcel.bodyInt[0] = (int)ratioW;
    parcel.bodyInt[1] = (int)ratioH;
    parcel.bodyInt[2] = maxW;
    parcel.bodyInt[3] = maxH;
    sendUiEvent(parcel);
    return Result {};
}

void SubtitleServer::getSubDimension(int32_t sId, getSubDimension_cb _hidl_cb) {
    // TODO implement
    return Void();
}

Result SubtitleServer::setSubDimension(int32_t sId, int32_t width, int32_t height) {
    // TODO implement
    return Result {};
}

Result SubtitleServer::setSurfaceViewRect(int32_t sId, int32_t x, int32_t y, int32_t w, int32_t h) {
    SubtitleHidlParcel parcel;
    android::AutoMutex _l(mLock);
    parcel.msgType = (int)FallthroughUiCmd::CMD_UI_SET_SURFACERECT;
    parcel.bodyInt.resize(4);
    parcel.bodyInt[0] = x;
    parcel.bodyInt[1] = y;
    parcel.bodyInt[2] = w;
    parcel.bodyInt[3] = h;
    sendUiEvent(parcel);
    return Result {};
}
#endif

void SubtitleServer::sendSubtitleDisplayNotify(SubtitleHidlParcel &event) {
    android::AutoMutex _l(mLock);

    SUBTITLE_LOGI("onEvent event:%d, client size:%d", event.msgType, mCallbackClients.size());

    if (mFallbackPlayStarted && mFallbackCallback != nullptr) {
        // enabled fallback display and fallback displayer started, then
        mFallbackCallback->notifyDataCallback(event);
        //SUBTITLE_LOGE(!r.isOk(), "Error, call notifyDataCallback failed! client died?");
        return;
    }

    for (int i = 0; i < mCallbackClients.size(); i++) {
        if (mCallbackClients[i] != nullptr) {
            SUBTITLE_LOGI("%s, xxxxxxxxxxxxxcc client cookie:%d notifyCallback", __FUNCTION__, i);
            mCallbackClients[i]->notifyDataCallback(event);
            //SUBTITLE_LOGE(!r.isOk(), "Error, call notifyDataCallback failed! client died?");
        }
    }
}

void SubtitleServer::sendSubtitleEventNotify(SubtitleHidlParcel &event) {
    android::AutoMutex _l(mLock);
    if (mFallbackPlayStarted && mFallbackCallback != nullptr) {
        mFallbackCallback->eventNotify(event);
        //SUBTITLE_LOGE(!r.isOk(), "Error, call eventNotify failed! client died?");
    }

    int clientSize = mCallbackClients.size();

    SUBTITLE_LOGI("onEvent event:%d, client size:%d", event.msgType, clientSize);

    // check has valid client or not.
    bool hasEventClient = false;
    if (clientSize > 0) {
        for (int i = 0; i < clientSize; i++) {
            if (mCallbackClients[i] != nullptr) {
                hasEventClient = true;
                break;
            }
        }
    }

    // send DisplayEvent to clients
    if (hasEventClient > 0) {
        for (int i = 0; i < clientSize; i++) {
            if (mCallbackClients[i] != nullptr) {
                SUBTITLE_LOGI("%s, client cookie:%d notifyCallback", __FUNCTION__, i);
                mCallbackClients[i]->eventNotify(event);
                //SUBTITLE_LOGE(!r.isOk(), "Error, call notifyDataCallback failed! client died?");
            }
        }
    } else {
        // No client connected, try fallback display.
        /*if (mFallbackDisplayClient != nullptr) {
            if (ENABLE_LOG_PRINT) SUBTITLE_LOGI("fallback display event:%d, client size:%d", parcel.msgType, clientSize);
            mFallbackDisplayClient->notifyDisplayCallback(parcel);
        }*/
    }
}


void SubtitleServer::sendUiEvent(SubtitleHidlParcel &event) {
    //if (!mFallbackPlayStarted) {
    //    SUBTITLE_LOGE("UI event request not proceed, do you called uiShow()?");
    //    return;
    //}

    if (mFallbackCallback == nullptr) {
        SUBTITLE_LOGE("Error, no default fallback display registered!");
        return;
    }

    mFallbackCallback->uiCommandCallback(event);
}

bool SubtitleServer::isClosed(int32_t sId) {
    return mServiceClients.find(sId) == mServiceClients.end();
}

Result SubtitleServer::setRenderType(int sId, int renderType) {
    std::shared_ptr<SubtitleService> ss = getSubtitleService(sId);
    SUBTITLE_LOGI("CCCCCCCCCCCCCCCC %s ss=%p setRenderType=%d", __func__, ss.get(), renderType);

    if (ss == nullptr) {
        SUBTITLE_LOGE("setRenderType failed, can not find SubtitleService for id %d", sId);
        return Result::FAIL;
    }

    ss->setRenderType(renderType);
    return Result::OK;
}

/*

void SubtitleServer::debug(const hidl_handle& handle, const hidl_vec<hidl_string>& options) {
    SUBTITLE_LOGI("%s", __func__);
    if (handle != nullptr && handle->numFds >= 1) {
        int fd = handle->data[0];

        std::vector<std::string> args;
        for (size_t i = 0; i < options.size(); i++) {
            args.push_back(options[i]);
        }
        dump(fd, args);
        fsync(fd);
    }
    return Void();
}
void SubtitleServer::dump(int fd, const std::vector<std::string>& args) {
    android::Mutex::Autolock lock(mLock);
    SUBTITLE_LOGI("%s", __func__);

    int len = args.size();
    for (int i = 0; i < len; i ++) {
        std::string debugInfoAll("-a");
        std::string debugLevel("-l");
        std::string dumpraw("--dumpraw");
        std::string dumpmemory("-m");
        std::string help("-h");
        if (args[i] == debugInfoAll) {
            //print all subtitle server runtime status(include subtitle ui)
            //we need write an api to get information from subtitle ui process:
            dprintf(fd, "\n");
        }
        else if (args[i] == debugLevel) {
            if (i + 1 < len) {
                std::string levelStr(args[i+1]);
                int level = atoi(levelStr.c_str());
                //setLogLevel(level);

                dprintf(fd, "Setting log level to %d.\n", level);
                break;
            }
        } else if (args[i] == dumpmemory) {
            if (!android::dumpMemoryAddresses(fd)) {
                dprintf(fd, "Cannot get malloc information!\n");
                dprintf(fd, "Please executed this first[need su]:\n\n");
                dprintf(fd, "    setprop libc.debug.malloc.options backtrace\n");
                dprintf(fd, "    stop subtitleserver\n");
                dprintf(fd, "    start subtitleserver\n");
                dprintf(fd, "    setprop libc.debug.malloc.options \"\"\n\n\n");
            }
            return;
        } else if (args[i] ==dumpraw) {
                mDumpMaps |= (1 << SUBTITLE_DUMP_SOURCE);
                dprintf(fd, "dump raw data source enabled.\n");
                return;
        } else if (args[i] == help) {
            dprintf(fd,
                "subtitle server hwbinder service use to control the ctc/apk channel to subtitle ui \n"
                "usage: \n"
                "lshal debug vendor.amlogic.hardware.subtitleserver@1.0::ISubtitleServer/default -l value \n"
                "-a: dump all subtitle server and subtitle ui related information \n"
                "-l: set subtitle server debug level \n"
                "-m: dump memory alloc info [need restart subtitle with libc.debug.malloc.options=backtrace]"
                "-h: help \n");
            return;
        }
    }

    //dump client:
    dprintf(fd, "\n\n HIDL Service: \n");
    dprintf(fd, "--------------------------------------------------------------------------------------\n\n");
    dprintf(fd, "Subtitile Clients:\n");
    if (mFallbackCallback != nullptr) {
        dprintf(fd, "    FallbackDisplayCallback: (%p)%s\n", mFallbackCallback.get(), toString(mFallbackCallback).c_str());
    }
    int clientSize = mCallbackClients.size();
    dprintf(fd, "    EventCallback: count=%d\n", clientSize);
    for (int i = 0; i < clientSize; i++) {
        dprintf(fd, "        %d: (%p)%s\n", i, mCallbackClients[i].get(),
            mCallbackClients[i]==nullptr ? "null" : toString(mCallbackClients[i]).c_str());
    }

    // TODO: travsel each service.
    std::shared_ptr<SubtitleService>  ss = getSubtitleServiceLocked(0);
    if (ss != nullptr) {
        ss->dump(fd);
    }
}
*/

bool SubtitleServer::ClientMessageHandlerImpl::onSubtitleDisplayNotify(SubtitleHidlParcel &event) {
    SUBTITLE_LOGE("CallbackHandlerImpl onSubtitleDataEvent");
    mSubtitleServer->sendSubtitleDisplayNotify(event);
    return false;
}

bool SubtitleServer::ClientMessageHandlerImpl::onSubtitleEventNotify(SubtitleHidlParcel &event) {
    SUBTITLE_LOGE("CallbackHandlerImpl onSubtitleDataEvent");
    mSubtitleServer->sendSubtitleEventNotify(event);
    return false;
}

#if 0
void SubtitleServer::handleServiceDeath(uint32_t cookie) {
    SUBTITLE_LOGI("%s", __func__);
    {
        android::AutoMutex _l(mLock);
        /*if (cookie == FALLBACK_DISPLAY_COOKIE) {
            mFallbackDisplayClient->unlinkToDeath(mDeathRecipient);
            mFallbackDisplayClient = nullptr;
        } else */{
            mCallbackClients[cookie]->unlinkToDeath(mDeathRecipient);
            mCallbackClients[cookie] = nullptr;
        }
    }
    // TODO: handle which client exited
    // currently, only support 1 subs
    //mSubtitleService->stopSubtitle();
    //close();
}

void SubtitleServer::DeathRecipient::serviceDied(
        uint64_t cookie,
        const ::android::wp<::android::hidl::base::V1_0::IBase>& who) {
    SUBTITLE_LOGE("subtitleserver daemon client died cookie:%d", (int)cookie);

    ::android::sp<::android::hidl::base::V1_0::IBase> s = who.promote();
    SUBTITLE_LOGE("subtitleserver daemon client died who:%p", s.get());

    if (s != nullptr) {
        auto r = s->interfaceDescriptor([&](const hidl_string &types) {
                SUBTITLE_LOGE("subtitleserver daemon client, who=%s", types.c_str());
            });
        if (!r.isOk()) {
            SUBTITLE_LOGE("why?");
        }
    }
    uint32_t type = static_cast<uint32_t>(cookie);
    mSubtitleServer->handleServiceDeath(type);
}
#endif

// Methods from ::android::hidl::base::V1_0::IBase follow.

//ISubtitleServer* HIDL_FETCH_ISubtitleServer(const char* /* name */) {
//return new SubtitleServer();
//}
//
//}  // namespace implementation
//}  // namespace V1_0
//}  // namespace subtitleserver
//}  // namespace hardware
//}  // namespace amlogic
//}  // namespace vendor
