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

#ifndef SUBTILECLIENTLINUX_H
#define SUBTILECLIENTLINUX_H

#include <map>
#include <memory>

#include <iostream>
#include <vector>
#include "ISubtitleCallback.h"

using namespace android;
using android::IMemory;
using std::vector;
using std::string;

class IpcSocket;

#ifdef __cplusplus
extern "C" {
#endif

enum ConnectType : int32_t {
    TYPE_HAL = 0,
    TYPE_EXTEND,
    TYPE_TOTAL,
};

enum OpenType : int32_t {
    TYPE_MIDDLEWARE = 0,  // from middleware, which no UI.
    TYPE_APPSDK,          // use SubtitleManager java SDK
};
class SubtitleListener : public android::RefBase {
public:
    virtual ~SubtitleListener() {}

    virtual void onSubtitleEvent(const char *data, int size, int parserType,
                int x, int y, int width, int height,
                int videoWidth, int videoHeight, int cmd) = 0;

    virtual void onSubtitleDataEvent(int event, int id) = 0;

    virtual void onSubtitleAvail(int avail) = 0;
    virtual void onSubtitleAfdEvent(int avail, int playerid) = 0;
    virtual void onSubtitleDimension(int width, int height) = 0;
    virtual void onMixVideoEvent(int val) = 0;
    virtual void onSubtitleLanguage(std::string lang) = 0;
    virtual void onSubtitleInfo(int what, int extra) = 0;


    // Middleware API do not need, this transfer UI command to fallback displayer
    virtual void onSubtitleUIEvent(int uiCmd, const std::vector<int> &params) = 0;



    // sometime, server may crash, we need clean up in server side.
    virtual void onServerDied() = 0;
};


class SubtitleClientLinux
{
public:

    enum {
        CMD_START = IBinder::FIRST_CALL_TRANSACTION,
        CMD_SUBTITLE_ACTION = IBinder::FIRST_CALL_TRANSACTION + 1,
    };

 //   SubtitleClientLinux();
    ~SubtitleClientLinux();
    SubtitleClientLinux(bool isFallback, sp<SubtitleListener> listener, OpenType openType) ;

     int openConnection();
     int closeConnection();
    //int open(native_handle_t& handle, int32_t ioType, OpenType openType);
     bool open(const char*path, int ioType);
     bool open(int fd, int ioType);
    void initRemoteLocked();

    int close();
    int resetForSeek();
    int updateVideoPos(int64_t pos);


    int getTotalTracks();
    int getType();
    int getLanguage();
    int getSessionId();

    int setSubType(int32_t type);
    int setSubPid( int32_t pid);
    int setPageId( int32_t pageId);
    int setAncPageId( int32_t ancPageId);
    int setChannelId(int32_t channelId);
    int setClosedCaptionVfmt(int32_t vfmt);
    void setRenderType(int type);

    int ttControl(int cmd, int magazine, int page, int regionId, int param) ;
    int ttGoHome() ;
    int ttNextPage(int32_t dir) ;
    int ttNextSubPage(int32_t dir) ;
    int ttGotoPage(int32_t pageNo, int32_t subPageNo) ;

    int userDataOpen() ;
    int userDataClose() ;


    void prepareWritingQueue(int32_t size) ;

    void setCallback(const sp<ISubtitleCallback>& callback, ConnectType type) ;
    void setFallbackCallback(const sp<ISubtitleCallback>& callback, ConnectType type) ;
    void removeCallback();
    int setPipId( int32_t mode, int32_t id) ;
/*
    //============ for stand alone UI, proxy API =============
    Result show(int32_t sId) override;
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
    // ui related.
    // Below api only used for control standalone UI.
    // Thes UI is not recomendated, only for some native app/middleware
    // that cannot Android (Java) UI hierarchy.
    bool uiShow();
    bool uiHide();
    bool uiSetTextColor(int color);
    bool uiSetTextSize(int size);
    bool uiSetGravity(int gravity);
    bool uiSetTextStyle(int style);
    bool uiSetYOffset(int yOffset);
    bool uiSetImageRatio(float ratioW, float ratioH, int32_t maxW, int32_t maxH);
    bool uiGetSubDemision(int *pWidth, int *pHeight);
    bool uiSetSurfaceViewRect(int x, int y, int width, int height);
    virtual void notify(int msg, int ext1, int ext2, const Parcel *obj) ;
class SubtitleCallback : public BnSubtitleCallback{
    public:
        SubtitleCallback(sp<SubtitleListener> sl) : mListener(sl) {}
        ~SubtitleCallback() {mListener = nullptr;}
        virtual void notifyDataCallback( SubtitleHidlParcel& parcel) override;
        virtual void uiCommandCallback( SubtitleHidlParcel& parcel)  override ;
        virtual void eventNotify( SubtitleHidlParcel& parcel) override;
       virtual void notify(int msg, int ext1, int ext2, const Parcel *obj) override;

	private:
        sp<SubtitleListener> mListener;
    };


    void applyRenderType();

private:
    static int eventReceiver(int fd, void *selfData);
    int SendMethodCall(char *CmdString, native_handle_t* handle = nullptr);
    int SplitRetBuf(const char *commandData);
    char mRetBuf[128] = {0};
    std::string  mRet[10];
    int mSessionId = -1;
    static inline bool mIsFallback;
    sp<SubtitleListener> mSubtitleListener;

    OpenType mOpenType;
    int mRenderType = 1/*DIRECT_RENDER*/;

    sp<IBinder> mSubtitleServicebinder;
    std::shared_ptr<IpcSocket> mIpcSocket;

   sp<SubtitleCallback> mCallback;
};
#ifdef __cplusplus
}
#endif
#endif

