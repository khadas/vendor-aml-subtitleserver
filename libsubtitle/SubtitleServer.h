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

#pragma once

#include <map>
//#include <vendor/amlogic/hardware/subtitleserver/1.0/ISubtitleServer.h>

//#include <fmq/EventFlag.h>
//#include <fmq/MessageQueue.h>
//#include <hidl/MQDescriptor.h>
//#include <hidl/Status.h>

/*
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <binder/IServiceManager.h>
#include <binder/Binder.h>
#include <binder/Parcel.h>
*/

#include <utils/Mutex.h>
#include "SubtitleService.h"
#include <cutils/native_handle.h>
#include "SubtitleType.h"
#include "AndroidCallbackMessageQueue.h"
#include <utils/RefBase.h>
#include "ISubtitleCallback.h"

//enum Result : uint32_t {
//    OK,                  // Success
//    FAIL,                // Failure, unknown reason
//};

enum OpenType : int32_t {
    TYPE_MIDDLEWARE = 0,  // from middleware, which no UI.
    TYPE_APPSDK,          // use SubtitleManager java SDK
};

enum ConnectType : int32_t {
    TYPE_HAL = 0,
    TYPE_EXTEND,
    TYPE_TOTAL,
};
/*
class ISubtitleCallback : public RefBase{
	public:
        ISubtitleCallback() {};
        virtual ~ISubtitleCallback() {};

    virtual void notifyDataCallback(SubtitleHidlParcel parcel) = 0;
    virtual void uiCommandCallback(SubtitleHidlParcel parcel) = 0;
    virtual void eventNotify(SubtitleHidlParcel parcel) = 0;
};
*/
// register a callback, for send to remote process and showing.
//using ::android::AndroidCallbackHandler;
using ::android::AndroidCallbackMessageQueue;

//namespace vendor {
//namespace amlogic {
//namespace hardware {
//namespace subtitleserver {
//namespace V1_0 {
//namespace implementation {

//using ::android::hardware::hidl_array;
//using ::android::hardware::hidl_handle;
//using ::android::hardware::hidl_memory;
//using ::android::hardware::hidl_string;
//using ::android::hardware::hidl_vec;
//using ::android::hardware::Return;
//using ::android::hardware::Void;
//using ::android::sp;

//using ::android::hardware::MessageQueue;/
//using ::android::hardware::kSynchronizedReadWrite;
//using ::android::hardware::EventFlag;

//typedef MessageQueue<uint8_t, kSynchronizedReadWrite> DataMQ;


class SubtitleServer  : public RefBase{
public:
    static SubtitleServer* Instance();

    SubtitleServer();
    // Methods from ISubtitleServer follow.
    int openConnection( ) ;
    Result closeConnection(int32_t sId) ;
    Result open(int32_t sId, int32_t ioType, OpenType openType, const native_handle_t* handle) ;
    Result close(int32_t sId) ;
    Result resetForSeek(int32_t sId) ;
    Result updateVideoPos(int32_t sId, int64_t pos) ;


    int getTotalTracks(int32_t sId) ;
    int getType(int32_t sId) ;
    std::string getLanguage(int32_t sId) ;
    bool isClosed(int32_t sId);

    Result setSubType(int32_t sId, int32_t type) ;
    Result setSubPid(int32_t sId, int32_t pid) ;
    Result setPageId(int32_t sId, int32_t pageId) ;
    Result setAncPageId(int32_t sId, int32_t ancPageId) ;
    Result setSecureLevel(int32_t sId, int32_t flag);
    Result setChannelId(int32_t sId, int32_t channelId) ;
    Result setClosedCaptionVfmt(int32_t sId, int32_t vfmt) ;

    Result ttControl(int32_t sId, int cmd, int magazine, int pageNo, int regionId, int param) ;
    Result ttGoHome(int32_t sId) ;
    Result ttNextPage(int32_t sId, int32_t dir) ;
    Result ttNextSubPage(int32_t sId, int32_t dir);
    Result ttGotoPage(int32_t sId, int32_t pageNo, int32_t subPageNo) ;

    Result userDataOpen(int32_t sId) ;
    Result userDataClose(int32_t sId) ;


    // void prepareWritingQueue(int32_t sId, int32_t size, prepareWritingQueue_cb _hidl_cb) ;

    void setCallback(const sp<ISubtitleCallback>& callback, ConnectType type) ;
   //  void setCallback( sp<ISubtitleCallback>& callback, ConnectType type) ;
   void setFallbackCallback(const sp<ISubtitleCallback>& callback, ConnectType type) ;
    void removeCallback(const sp<ISubtitleCallback>& callback);

    //============ for stand alone UI, proxy API =============
  /*  Result show(int32_t sId) override;
    Result hide(int32_t sId) override;
    Result setTextColor(int32_t sId, int32_t color) override;
    Result setTextSize(int32_t sId, int32_t size) override;
    Result setGravity(int32_t sId, int32_t gravity) override;
    Result setTextStyle(int32_t sId, int32_t style) override;
    Result setPosHeight(int32_t sId, int32_t height) override;
    Result setImgRatio(int32_t sId, float ratioW, float ratioH, int32_t maxW, int32_t maxH) override;
    void getSubDemision(int32_t sId, getSubDemision_cb _hidl_cb) override;
    Result setSubDemision(int32_t sId, int32_t width, int32_t height) override;
    Result setSurfaceViewRect(int32_t sId, int32_t x, int32_t y, int32_t w, int32_t h) override;
    Result setPipId(int32_t sId, int32_t mode, int32_t id) override;
*/
    Result setPipId(int32_t sId, int32_t mode, int32_t id) ;
    // Methods from ::android::hidl::base::V1_0::IBase follow.
    //void debug(const hidl_handle& fd, const hidl_vec<hidl_string>& args) override;

    Result setRenderType(int sId, int renderType);

private:
   // void dump(int fd, const std::vector<std::string>& args);
    int mDumpMaps;

    std::shared_ptr<SubtitleService> getSubtitleServiceLocked(int sId);
    std::shared_ptr<SubtitleService> getSubtitleService(int sId);
    void handleServiceDeath(uint32_t cookie);

    // TODO: merge the callbacks?
    void sendSubtitleDisplayNotify(SubtitleHidlParcel &event);
    void sendSubtitleEventNotify(SubtitleHidlParcel &event);

    void sendUiEvent(SubtitleHidlParcel &event);

    mutable android::Mutex mLock;

    int mCurSessionId;
    std::map<uint32_t, std::shared_ptr<SubtitleService>> mServiceClients;

    /* FMQ for media send softdemuxed subtitle data */
   // std::unique_ptr<DataMQ> mDataMQ;


    std::map<uint32_t,  sp<ISubtitleCallback>> mCallbackClients;
    sp<ISubtitleCallback> mFallbackCallback;


    class ClientMessageHandlerImpl : public AndroidCallbackHandler {
    public:
        ClientMessageHandlerImpl(sp<SubtitleServer> ssh) : mSubtitleServer(ssh) {}
        bool onSubtitleDisplayNotify(SubtitleHidlParcel &event);
        bool onSubtitleEventNotify(SubtitleHidlParcel &event);
    private:
        sp<SubtitleServer> mSubtitleServer;
    };
    sp<AndroidCallbackMessageQueue> mMessageQueue;

  /*  class DeathRecipient : public android::hardware::hidl_death_recipient  {
    public:
        DeathRecipient(sp<SubtitleServer> ssh) : mSubtitleServer(ssh) {}

        // hidl_death_recipient interface
        virtual void serviceDied(uint64_t cookie,
            const ::android::wp<::android::hidl::base::V1_0::IBase>& who) override;

    private:
        sp<SubtitleServer> mSubtitleServer;
    };

    sp<DeathRecipient> mDeathRecipient;
*/
    static SubtitleServer* sInstance;

    bool mFallbackPlayStarted;

    // avoid CTC or other native impl called in middleware which is used by VideoPlayer
    // then VideoPlayer and middleware both call subtitle, but open related api we want
    // middleware's, other configuration related can from VideoPlayer.
    bool mOpenCalled;
    OpenType mLastOpenType;
    int64_t mLastOpenTime;
};

// FIXME: most likely delete, this is only for passthrough implementations
// extern "C" ISubtitleServer* HIDL_FETCH_ISubtitleServer(const char* name);

//}  // namespace implementation
//}  // namespace V1_0
//}  // namespace subtitleserver
//}  // namespace hardware
//}  // namespace amlogic
//}  // namespace vendor
