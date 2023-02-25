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

#define LOG_MOUDLE_TAG "Subtitle"
#define LOG_CLASS_TAG "SubtitleClientLinux"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <memory>

#include "SubtitleClientLinux.h"
#include "subtitlecmd.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <EventsTracker.h>
#include <IpcSocket.h>

#define SERVER_PORT 10200

#define format_string(format, ...) \
    ({char fmt_buf[256] = {0}; sprintf(fmt_buf, format, ##__VA_ARGS__); fmt_buf;})

const int RET_SUCCESS = 0;
const int RET_FAILED = -1;

const int EVENT_SIGLE_DETECT = 4;
const int EVENT_SOURCE_CONNECT = 10;

SubtitleClientLinux *mInstance = NULL;

SubtitleClientLinux::SubtitleClientLinux(bool isFallback, sp<SubtitleListener> listener,
                                         OpenType openType) {
    ALOGD("SubtitleServerHidlClient getSubtitleService ...");
    // Mutex::Autolock _l(mLock);
    mIsFallback = isFallback;
    mSubtitleListener = listener;
    mOpenType = openType;

    initRemoteLocked();
}

void SubtitleClientLinux::initRemoteLocked() {
    if (mIpcSocket == nullptr) {
        mIpcSocket = std::make_shared<IpcSocket>(false);
        mIpcSocket->start(SERVER_PORT, nullptr);
    }

    if (!mIpcSocket->isvalid()) {
        ALOGE("Client: initRemoteLocked failed");
        mIpcSocket->stop();
        return;
    }

    mIpcSocket->setEventListener(eventReceiver);

    mSessionId = openConnection();
    ALOGD("Connected to subtitleservice.  mSessionId = %d\n", mSessionId);
}

/*static */
int SubtitleClientLinux::eventReceiver(int fd, void */*selfData*/) {
    char retBuf[1024];
    if (!IpcSocket::receive(fd, retBuf, sizeof(retBuf))) {
        ALOGD("Server broken, fd= %d, remove.", fd);
        return EventsTracker::RET_REMOVE;
    } else {
        ALOGD("Client received: %s", retBuf);
    }

    return EventsTracker::RET_CONTINUE;
}

SubtitleClientLinux::~SubtitleClientLinux() {
    Parcel send, reply;
    //mSubtitleServicebinder->transact(CMD_CLR_PQ_CB, send, &reply);
    closeConnection();
    mSubtitleServicebinder = nullptr;

    if (mIpcSocket != nullptr) {
        mIpcSocket->stop();
        mIpcSocket->join();
    }
}

int SubtitleClientLinux::SendMethodCall(char *CmdString, native_handle_t* /*handle*/) {
    int ret = -1;
    if (mIpcSocket != nullptr) {
        std::string payload(CmdString);
        int payloadSize = payload.size();

        auto resultStr = mIpcSocket->sendForResult(mIpcSocket->fd(), payload.c_str(), payloadSize);
        if (resultStr == "-1") {
            ALOGE("Server is broken, stop!!!");
            mIpcSocket->stop();
            return ret;
        }

        if (!resultStr.empty()) {
            ALOGD("[%s]  %s ==> ||| <== %s", __FUNCTION__ , CmdString, resultStr.c_str());
            ret = atoi(resultStr.c_str());
        }
    }

    return ret;
}

int SubtitleClientLinux::SplitRetBuf(const char *commandData) {
    char cmdbuff[1024];
    char *token;
    int cmd_size = 0;
    const char *delimitation = ".";

    strcpy(cmdbuff, commandData);

    /* get first str*/
    token = strtok(cmdbuff, delimitation);

    /* continue get str*/
    while (token != NULL) {
        mRet[cmd_size].assign(token);

        ALOGD("%s mRet[%d]:%s\n", token, cmd_size, mRet[cmd_size].c_str());
        cmd_size++;
        token = strtok(NULL, delimitation);
    }

    ALOGD("%s: cmd_size = %d\n", __FUNCTION__, cmd_size);

    return cmd_size;
}

int SubtitleClientLinux::openConnection() {
    ALOGD("%s\n", __FUNCTION__);
    int ret = -1;
    int sessionId = SendMethodCall(format_string("subtitle.ctrl.%d", SUBTITLE_OPENCONNECTION));
    if (sessionId >= 0) {
        mSessionId = sessionId;
        ret = 0;
    }
    return ret;
}

int SubtitleClientLinux::closeConnection() {
    ALOGD("%s\n", __FUNCTION__);
    int ret = SendMethodCall(
            format_string("subtitle.ctrl.%d.%d", SUBTITLE_CLOSECONNECTION, mSessionId));
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;
}

bool SubtitleClientLinux::open(const char *path, int ioType) {
    ALOGD("%s\n", __FUNCTION__);
    return true;
}

bool SubtitleClientLinux::open(int fd, int ioType) {
    ALOGD(" SubtitleClientLinux  %s, line %d fd=%d,ioType=%d", __FUNCTION__, __LINE__, fd, ioType);
    char buf[32] = {0};
    //int param = (demuxId << 16 | iotType);
    native_handle_t *nativeHandle = nullptr;
    if (fd > 0) {
        ::lseek(fd, 0, SEEK_SET);
        nativeHandle = native_handle_create(1, 0);
        nativeHandle->data[0] = fd;
    }
    int ret = SendMethodCall(format_string("subtitle.ctrl.%d.%d.%d.%d",
                                           SUBTITLE_OPEN, mSessionId, ioType,
                                           mOpenType), nativeHandle);
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return true;
}

int SubtitleClientLinux::close() {
    ALOGD("%s\n", __FUNCTION__);
    int ret = SendMethodCall(format_string("subtitle.ctrl.%d.%d", SUBTITLE_CLOSE, mSessionId));
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;

}

int SubtitleClientLinux::resetForSeek() {
    int ret = SendMethodCall(
            format_string("subtitle.ctrl.%d.%d", SUBTITLE_RESETFORSEEK, mSessionId));
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;

}

int SubtitleClientLinux::updateVideoPos(int32_t pos) {
    int ret = SendMethodCall(
            format_string("subtitle.ctrl.%d.%d.%d", SUBTITLE_UPDATEVIDEOPTS, mSessionId,
                          pos));
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;
}

int SubtitleClientLinux::getTotalTracks() {
    int ret = SendMethodCall(
            format_string("subtitle.get.%d.%d", SUBTITLE_GETTOTALTRACKS, mSessionId));
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;
}

int SubtitleClientLinux::getType() {
    int ret = SendMethodCall(format_string("subtitle.get.%d.%d", SUBTITLE_GETTYPE, mSessionId));
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;
}

int SubtitleClientLinux::getLanguage() {
    int ret = SendMethodCall(format_string("subtitle.get.%d.%d", SUBTITLE_GETLANGUAGE, mSessionId));
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;
}

int SubtitleClientLinux::getSessionId() {
    return mSessionId;
}

int SubtitleClientLinux::setSubType(int32_t type) {
    int ret = SendMethodCall(
            format_string("subtitle.set.%d.%d.%d", SUBTITLE_SETSUBTYPE, mSessionId, type));
    return ret;

}

int SubtitleClientLinux::setSubPid(int32_t pid) {
    int ret = SendMethodCall(
            format_string("subtitle.set.%d.%d.%d", SUBTITLE_GETSUBPID, mSessionId, pid));
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;
}

int SubtitleClientLinux::setPageId(int32_t pageId) {
    int ret = SendMethodCall(
            format_string("subtitle.set.%d.%d.%d", SUBTITLE_SETPAGEID, mSessionId, pageId));
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;
}

int SubtitleClientLinux::setAncPageId(int32_t ancPageId) {
    int ret = SendMethodCall(
            format_string("subtitle.set.%d.%d.%d", SUBTITLE_SETANCPAGEID, mSessionId, ancPageId));
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;
}

int SubtitleClientLinux::setChannelId(int32_t channelId) {
    int ret = SendMethodCall(
            format_string("subtitle.set.%d.%d.%d", SBUTITLE_SETCHANNELID, mSessionId, channelId));
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;
}

int SubtitleClientLinux::setClosedCaptionVfmt(int32_t vfmt) {
    int ret = SendMethodCall(
            format_string("subtitle.set.%d", SBUTITLE_SETCHANNELID, mSessionId, vfmt));
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;
}

int SubtitleClientLinux::ttControl(int cmd, int magazine, int page, int regionId, int param) {
    int ret = SendMethodCall(
            format_string("subtitle.ttcontrol.%d.%d.%d.%d.%d.%d.%d",
                          SUBTITLE_TTCONTROL, mSessionId, cmd, magazine, page, regionId, param));
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;
}

int SubtitleClientLinux::ttGoHome() {
    int ret = SendMethodCall(format_string("subtitle.ttcontrol.%d.%d",
                                           SUBTITLE_TTGOHOME, mSessionId));
    ALOGE("SubtitleClientLinux: ret %d\n", ret);
    return ret;
}

int SubtitleClientLinux::ttNextPage(int32_t dir) {
    int ret = SendMethodCall(format_string("subtitle.ttcontrol.%d.%d.%d",
                                           SUBTITLE_TTNEXTPAGE, mSessionId, dir));
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;

}

int SubtitleClientLinux::ttNextSubPage(int32_t dir) {
    int ret = SendMethodCall(
            format_string("subtitle.ttcontrol.%d.%d.%d", SUBTITLE_TTNEXTSUBPAGE, mSessionId, dir));
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;
}

int SubtitleClientLinux::ttGotoPage(int32_t pageNo, int32_t subPageNo) {
    int ret = SendMethodCall(format_string("subtitle.ttcontrol.%d.%d.%d.%d",
                                           SUBTITLE_TTGOTOPAGE, mSessionId, pageNo, subPageNo));
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;
}

int SubtitleClientLinux::userDataOpen() {
    int ret = SendMethodCall(format_string("subtitle.afdcontrol.%d.%d",
                                           SUBTITLE_USERDATAOPEN, mSessionId));
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;
}

int SubtitleClientLinux::userDataClose() {
    int ret = SendMethodCall(format_string("subtitle.afdcontrol.%d.%d",
                                           SUBTITLE_USERDATACLOSE, mSessionId));
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;
}

int SubtitleClientLinux::setPipId(int32_t mode, int32_t id) {
    int ret = SendMethodCall(format_string("subtitle.set.%d.%d.%d.%d",
                                           SUBTITLE_SETPIPID, mSessionId, mode, id));
    ALOGE("SubtitleClientLinux: setPipId, ret %d.\n", ret);
    return ret;
}

// ui related.
// Below api only used for control standalone UI.
// Thes UI is not recomendated, only for some native app/middleware
// that cannot Android (Java) UI hierarchy.
bool SubtitleClientLinux::uiShow() {
    return true;
}

bool SubtitleClientLinux::uiHide() {
    return true;
}

bool SubtitleClientLinux::uiSetTextColor(int color) {
    return true;
}

bool SubtitleClientLinux::uiSetTextSize(int size) {
    return true;
}


bool SubtitleClientLinux::uiSetGravity(int gravity) {
    return true;
}

bool SubtitleClientLinux::uiSetTextStyle(int style) {
    return true;
}

bool SubtitleClientLinux::uiSetYOffset(int yOffset) {
    return true;
}

bool SubtitleClientLinux::uiSetImageRatio(float ratioW, float ratioH, int32_t maxW, int32_t maxH) {
    return true;
}

bool SubtitleClientLinux::uiGetSubDemision(int *pWidth, int *pHeight) {
    return true;
}

bool SubtitleClientLinux::uiSetSurfaceViewRect(int x, int y, int width, int height) {
    return true;
}


void SubtitleClientLinux::prepareWritingQueue(int32_t size) {
/*    char buf[32] = {0};
    int  ret     = -1;

    sprintf(buf, "subtitle.openConnection.%d", SUBTITLE_PREPAREWRITINGQUEUE);
    SendMethodCall(buf);

    ret = atoi(mRetBuf);
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);

    return ret;
*/
}

void SubtitleClientLinux::notify(int msg, int ext1, int ext2, const Parcel *obj) {
    return;
}

void SubtitleClientLinux::setCallback(const sp<ISubtitleCallback> &callback, ConnectType type) {
    /*  char buf[32] = {0};
       int  ret     = -1;

       sprintf(buf, "subtitle.callback.%d", SUBTITLE_SETFALLCALLBACK);
       SendMethodCall(buf);

       ret = atoi(mRetBuf);
       ALOGE("SubtitleClientLinux: ret %d.\n", ret);
       send.writeStrongBinder(sp<IBinder>(this));
       tvServicebinder->transact(CMD_SET_TV_CB, send, &reply);
   */
    //return ret;
}

void
SubtitleClientLinux::setFallbackCallback(const sp<ISubtitleCallback> &callback, ConnectType type) {
    /*  char buf[32] = {0};
       int  ret     = -1;

       sprintf(buf, "subtitle.addCallback.%d", SUBTITL_);
       SendMethodCall(buf);

       ret = atoi(mRetBuf);
       ALOGE("SubtitleClientLinux: ret %d.\n", ret);

       return ret;
   */
}

void SubtitleClientLinux::removeCallback() {
    char buf[32] = {0};
    int ret = -1;

    sprintf(buf, "subtitle.callback.%d", SUBTITLE_REMOVECALLBACK);
    SendMethodCall(buf);

    ret = atoi(mRetBuf);
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);

    //return ret;

}

void SubtitleClientLinux::setRenderType(int type) {
    mRenderType = type;
}

void SubtitleClientLinux::applyRenderType() {
    int ret = SendMethodCall(format_string("subtitle.set.%d.%d.%d",
                                           SUBTITLE_SETRENDERTYPE, mSessionId, mRenderType));
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
}

void SubtitleClientLinux::SubtitleCallback::notify(int msg, int ext1, int ext2, const Parcel *obj) {
    ALOGE(" SubtitleClientLinux::notify");
}

#if 1
void SubtitleClientLinux::SubtitleCallback::notifyDataCallback(SubtitleHidlParcel &parcel) {
    //hidl_memory mem = parcel.mem;
    int type = parcel.msgType;
    //sp<IMemory> memory = mapMemory(mem);
    int width = parcel.bodyInt[0];
    int height = parcel.bodyInt[1];
    int size = parcel.bodyInt[2];
    int cmd = parcel.bodyInt[3];
    int cordinateX = parcel.bodyInt[4];
    int cordinateY = parcel.bodyInt[5];
    int videoWidth = parcel.bodyInt[6];
    int videoHeight = parcel.bodyInt[7];
    int keyid = parcel.shmid;
    ALOGD("processSubtitleData! aa %d %d,width=%d,height=%d", type, parcel.msgType, width, height);
    key_t key;
    char pathname[30];
    strcpy(pathname, "/tmp");
    int shmId = shmget(6549, size, 0);
    if (shmId == -1) {
        ALOGE("Error get shm  ipc_id error shmid=%d, error=%s", shmId, strerror(errno));
        return;
    }
    char *data = (char *) shmat(shmId, NULL, 0);
    if (mListener != nullptr) {
        mListener->onSubtitleEvent(data, size, type, cordinateX, cordinateY, width, height,
                                   videoWidth, videoHeight, cmd);
    } else {
        ALOGD("error, no handle for this event!");
    }
    shmdt(data);
    shmctl(shmId, IPC_RMID, NULL);
}

void SubtitleClientLinux::SubtitleCallback::eventNotify(SubtitleHidlParcel &parcel) {

}

void SubtitleClientLinux::SubtitleCallback::uiCommandCallback(SubtitleHidlParcel &parcel) {

}
#endif
