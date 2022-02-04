/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#define LOG_MOUDLE_TAG "Subtitle"
#define LOG_CLASS_TAG "SubtitleClientLinux"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>

#include "SubtitleClientLinux.h"
#include "subtitlecmd.h"
#include <sys/ipc.h>
#include <sys/shm.h>

#define format_string(format, ...) \
    ({char fmt_buf[32] = {0}; sprintf(fmt_buf, format, ##__VA_ARGS__); fmt_buf;})

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
#ifndef RDK_AML_SUBTITLE_SOCKET
void SubtitleClientLinux::initRemoteLocked() {
    sp<ProcessState> proc(ProcessState::self());
    proc->startThreadPool();
    Parcel send, reply;
    sp<IServiceManager> serviceManager = defaultServiceManager();
    do {
        mSubtitleServicebinder = serviceManager->getService(String16("subtitleservice"));
        if (mSubtitleServicebinder != 0) {
            ALOGD(" SubtitleClientLinux  %s, line %d :: succefully to get the subtitle service",
                  __FUNCTION__, __LINE__);
            break;
        }

        ALOGE("Get subtitleservice later 5000ms");
        usleep(5000);
    } while (true);
    mCallback = new SubtitleClientLinux::SubtitleCallback(mSubtitleListener);
    if (mIsFallback) {
        ALOGD("regist fallback subtitle");
        //setFallbackCallback(mCallback, static_cast<ConnectType>(1));
    } else {
        Parcel send, reply;
        send.writeStrongBinder(IInterface::asBinder(mCallback));
        mSubtitleServicebinder->transact(SUBTITLE_SETFALLCALLBACK, send, &reply);

    }

    mSessionId = openConnection();
    ALOGD("Connected to subtitleservice.  mSessionId = %d\n", mSessionId);
}

SubtitleClientLinux::~SubtitleClientLinux() {
    Parcel send, reply;
    //mSubtitleServicebinder->transact(CMD_CLR_PQ_CB, send, &reply);
    closeConnection();
    mSubtitleServicebinder = nullptr;
}

int SubtitleClientLinux::SendMethodCall(char *CmdString, native_handle_t* handle) {
    int ret = -1;
    Parcel send, reply;
    if (mSubtitleServicebinder != nullptr) {
        send.writeCString(CmdString);
        if (handle != nullptr) {
            send.writeNativeHandle(handle);
        }
        if (mSubtitleServicebinder->transact(CMD_SUBTITLE_ACTION, send, &reply) != 0) {
            ALOGE("SubtitleClientLinux: call %s failed\n", CmdString);
        }
        ret = reply.readInt32();
    }
    return ret;
}
#endif //RDK_AML_SUBTITLE_SOCKET
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

#ifndef RDK_AML_SUBTITLE_SOCKET
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
            format_string("subtitle.ttcontrol.%d",
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
#endif //RDK_AML_SUBTITLE_SOCKET

int SubtitleClientLinux::setPipId(int32_t mode, int32_t id) {
    return 0;
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

//void  SubtitleClientLinux::removeCallback(const sp<ISubtitleCallback>& callback){
void SubtitleClientLinux::removeCallback() {
    char buf[32] = {0};
    int ret = -1;

    sprintf(buf, "subtitle.callback.%d", SUBTITLE_REMOVECALLBACK);
    #ifdef RDK_AML_SUBTITLE_SOCKET
    SendMethodCall(buf, sizeof(buf));
    #else
    SendMethodCall(buf);
    #endif //RDK_AML_SUBTITLE_SOCKET

    ret = atoi(mRetBuf);
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);

    //return ret;

}

void SubtitleClientLinux::setRenderType(int type) {
    mRenderType = type;
}
#ifndef RDK_AML_SUBTITLE_SOCKET
void SubtitleClientLinux::applyRenderType() {
    int ret = SendMethodCall(format_string("subtitle.set.%d.%d.%d",
                                           SUBTITLE_SETRENDERTYPE, mSessionId, mRenderType));
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
}
#endif //RDK_AML_SUBTITLE_SOCKET
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

#ifdef RDK_AML_SUBTITLE_SOCKET

void SubtitleClientLinux::initRemoteLocked() {
    sp<ProcessState> proc(ProcessState::self());
    proc->startThreadPool();
    Parcel send, reply;
    sp<IServiceManager> serviceManager = defaultServiceManager();

    // Binder parts are removed, sockets are used in this proof-of-concept
    // Binder is available/not working on RDK, sockets are used instead
    /*
    do {
        mSubtitleServicebinder = serviceManager->getService(String16("subtitleservice"));
        if (mSubtitleServicebinder != 0) {
            ALOGD(" SubtitleClientLinux  %s, line %d :: succefully to get the subtitle service",
                  __FUNCTION__, __LINE__);
            break;
        }

        ALOGE("Get subtitleservice later 5000ms");
        usleep(5000);
    } while (true);
    mCallback = new SubtitleClientLinux::SubtitleCallback(mSubtitleListener);
    if (mIsFallback) {
        ALOGD("regist fallback subtitle");
        //setFallbackCallback(mCallback, static_cast<ConnectType>(1));
    } else {
        Parcel send, reply;
        send.writeStrongBinder(IInterface::asBinder(mCallback));
        mSubtitleServicebinder->transact(SUBTITLE_SETFALLCALLBACK, send, &reply);

    }
*/
    if (nullptr == mAmSubtitle) {
        mAmSubtitle = new AmSubtitle();
        mAmSubtitle->init();
        mAmSubtitle->connectCmdSocket();
    }

    openConnection();
    ALOGD("Connected to subtitleservice.  mSessionId = %d\n", mSessionId);
}

SubtitleClientLinux::~SubtitleClientLinux() {
    Parcel send, reply;
    //mSubtitleServicebinder->transact(CMD_CLR_PQ_CB, send, &reply);
    closeConnection();
    delete mAmSubtitle;
}

void SubtitleClientLinux::SendMethodCall(char *CmdString, size_t cmdSize, native_handle_t* handle) {
    int ret = -1;
    size_t buffSize = sizeof(mRetBuf)/sizeof(char);
    memset(mRetBuf, 0, buffSize);
    /*
    Parcel send, reply;
    if (mSubtitleServicebinder != nullptr) {
        send.writeCString(CmdString);
        if (handle != nullptr) {
            send.writeNativeHandle(handle);
        }
        if (mSubtitleServicebinder->transact(CMD_SUBTITLE_ACTION, send, &reply) != 0) {
            ALOGE("SubtitleClientLinux: call %s failed\n", CmdString);
        }
        ret = reply.readInt32();
    }
    */

    ret = mAmSubtitle->sendCmdToSubtitleService((uint8_t*)CmdString, cmdSize);
    if (ret < 0) {
       return;
    }
    int recv = mAmSubtitle->recvCmdFromSubtitleService((uint8_t*)mRetBuf, buffSize);
    if (recv == -1) {
        LOGE("SubtitleClientLinux: received command from subtitle server failed!\n");
    }
}

int SubtitleClientLinux::openConnection() {
    ALOGD("%s\n", __FUNCTION__);
    int ret = -1;
    SendMethodCall(format_string("subtitle.ctrl.%d", SUBTITLE_OPENCONNECTION), sizeof("subtitle.ctrl.%d"));
    mSessionId = atoi(mRetBuf);
    /*
    if (sessionId >= 0) {
        mSessionId = sessionId;
        ret = 0;
    }
    */
    return ret;
}

int SubtitleClientLinux::closeConnection() {
    ALOGD("%s\n", __FUNCTION__);
    SendMethodCall(
            format_string("subtitle.ctrl.%d.%d", SUBTITLE_CLOSECONNECTION, mSessionId), sizeof("subtitle.ctrl.%d.%d"));
    int ret = atoi(mRetBuf);
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
    SendMethodCall(format_string("subtitle.ctrl.%d.%d.%d.%d",
                                           SUBTITLE_OPEN, mSessionId, ioType,
                                           mOpenType), sizeof("subtitle.ctrl.%d.%d.%d.%d"), nativeHandle);
    int ret = atoi(mRetBuf);
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return true;
}

int SubtitleClientLinux::close() {
    ALOGD("%s\n", __FUNCTION__);
    SendMethodCall(format_string("subtitle.ctrl.%d.%d", SUBTITLE_CLOSE, mSessionId), sizeof("subtitle.ctrl.%d.%d"));
    int ret = atoi(mRetBuf);
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;

}

int SubtitleClientLinux::resetForSeek() {
    SendMethodCall(
            format_string("subtitle.ctrl.%d.%d", SUBTITLE_RESETFORSEEK, mSessionId), sizeof("subtitle.ctrl.%d.%d"));
    int ret = atoi(mRetBuf);
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;

}

int SubtitleClientLinux::updateVideoPos(int32_t pos) {
    SendMethodCall(
            format_string("subtitle.ctrl.%d.%d.%d", SUBTITLE_UPDATEVIDEOPTS, mSessionId,
                          pos), sizeof("subtitle.ctrl.%d.%d.%d"));
    int ret = atoi(mRetBuf);
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;

}

int SubtitleClientLinux::getTotalTracks() {
    SendMethodCall(
            format_string("subtitle.get.%d.%d", SUBTITLE_GETTOTALTRACKS, mSessionId), sizeof("subtitle.get.%d.%d"));
    int ret = atoi(mRetBuf);
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;
}

int SubtitleClientLinux::getType() {
    SendMethodCall(format_string("subtitle.get.%d.%d", SUBTITLE_GETTYPE, mSessionId), sizeof("subtitle.get.%d.%d"));
    int ret = atoi(mRetBuf);
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;

}

int SubtitleClientLinux::getLanguage() {
    SendMethodCall(format_string("subtitle.get.%d.%d", SUBTITLE_GETLANGUAGE, mSessionId), sizeof("subtitle.get.%d.%d"));
    int ret = atoi(mRetBuf);
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;

}

int SubtitleClientLinux::getSessionId() {
    return mSessionId;
}

int SubtitleClientLinux::setSubType(int32_t type) {
    SendMethodCall(
            format_string("subtitle.set.%d.%d.%d", SUBTITLE_SETSUBTYPE, mSessionId, type), sizeof("subtitle.set.%d.%d.%d"));
    int ret = atoi(mRetBuf);
    return ret;

}

int SubtitleClientLinux::setSubPid(int32_t pid) {
    SendMethodCall(
            format_string("subtitle.set.%d.%d.%d", SUBTITLE_GETSUBPID, mSessionId, pid), sizeof("subtitle.set.%d.%d.%d"));
    int ret = atoi(mRetBuf);
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;
}

int SubtitleClientLinux::setPageId(int32_t pageId) {
    SendMethodCall(
            format_string("subtitle.set.%d.%d.%d", SUBTITLE_SETPAGEID, mSessionId, pageId), sizeof("subtitle.set.%d.%d.%d"));
    int ret = atoi(mRetBuf);
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;
}

int SubtitleClientLinux::setAncPageId(int32_t ancPageId) {
    SendMethodCall(
            format_string("subtitle.set.%d.%d.%d", SUBTITLE_SETANCPAGEID, mSessionId, ancPageId), sizeof("subtitle.set.%d.%d.%d"));
    int ret = atoi(mRetBuf);
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;
}

int SubtitleClientLinux::setChannelId(int32_t channelId) {
    SendMethodCall(
            format_string("subtitle.set.%d.%d.%d", SBUTITLE_SETCHANNELID, mSessionId, channelId), sizeof("subtitle.set.%d.%d.%d"));
    int ret = atoi(mRetBuf);
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;

}

int SubtitleClientLinux::setClosedCaptionVfmt(int32_t vfmt) {
    SendMethodCall(
            format_string("subtitle.set.%d", SBUTITLE_SETCHANNELID, mSessionId, vfmt), sizeof("subtitle.set.%d"));
    int ret = atoi(mRetBuf);
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;
}

int SubtitleClientLinux::ttControl(int cmd, int magazine, int page, int regionId, int param) {
    SendMethodCall(
            format_string("subtitle.ttcontrol.%d",
                          SUBTITLE_TTCONTROL, mSessionId, cmd, magazine, page, regionId, param), sizeof("subtitle.ttcontrol.%d"));
    int ret = atoi(mRetBuf);
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;
}

int SubtitleClientLinux::ttGoHome() {
    SendMethodCall(format_string("subtitle.ttcontrol.%d.%d",
                                           SUBTITLE_TTGOHOME, mSessionId), sizeof("subtitle.ttcontrol.%d.%d"));
    int ret = atoi(mRetBuf);
    ALOGE("SubtitleClientLinux: ret %d\n", ret);
    return ret;
}

int SubtitleClientLinux::ttNextPage(int32_t dir) {
    SendMethodCall(format_string("subtitle.ttcontrol.%d.%d.%d",
                                           SUBTITLE_TTNEXTPAGE, mSessionId, dir), sizeof("subtitle.ttcontrol.%d.%d.%d"));
    int ret = atoi(mRetBuf);
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;

}

int SubtitleClientLinux::ttNextSubPage(int32_t dir) {
    SendMethodCall(
            format_string("subtitle.ttcontrol.%d.%d.%d", SUBTITLE_TTNEXTSUBPAGE, mSessionId, dir), sizeof("subtitle.ttcontrol.%d.%d.%d"));
    int ret = atoi(mRetBuf);
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;
}

int SubtitleClientLinux::ttGotoPage(int32_t pageNo, int32_t subPageNo) {
    SendMethodCall(format_string("subtitle.ttcontrol.%d.%d.%d.%d",
                                           SUBTITLE_TTGOTOPAGE, mSessionId, pageNo, subPageNo), sizeof("subtitle.ttcontrol.%d.%d.%d.%d"));
    int ret = atoi(mRetBuf);
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;
}

int SubtitleClientLinux::userDataOpen() {
    SendMethodCall(format_string("subtitle.afdcontrol.%d.%d",
                                           SUBTITLE_USERDATAOPEN, mSessionId), sizeof("subtitle.afdcontrol.%d.%d"));
    int ret = atoi(mRetBuf);
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;
}

int SubtitleClientLinux::userDataClose() {
    SendMethodCall(format_string("subtitle.afdcontrol.%d.%d",
                                           SUBTITLE_USERDATACLOSE, mSessionId), sizeof("subtitle.afdcontrol.%d.%d"));
    int ret = atoi(mRetBuf);
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
    return ret;
}

void SubtitleClientLinux::applyRenderType() {
    SendMethodCall(format_string("subtitle.set.%d.%d.%d",
                                           SUBTITLE_SETRENDERTYPE, mSessionId, mRenderType), sizeof("subtitle.set.%d.%d.%d"));
    int ret = atoi(mRetBuf);
    ALOGE("SubtitleClientLinux: ret %d.\n", ret);
}
#endif //RDK_AML_SUBTITLE_SOCKET
