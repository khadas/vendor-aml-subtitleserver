/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#define LOG_TAG "SubtitleServiceLinux"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <cutils/native_handle.h>
#include "SubtitleServiceLinux.h"
#include "subtitlecmd.h"
#include "SubtitleServer.h"
#include <IpcSocket.h>
#include <EventsTracker.h>
#include <SubtitleSignalHandler.h>

#define SERVER_PORT 10200

#ifdef __cplusplus
extern "C" {
#endif

SubtitleServiceLinux *mInstance = NULL;
SubtitleServiceLinux *SubtitleServiceLinux::GetInstance() {
            LOGD(" SubtitleServiceLinux  %s, line %d", __FUNCTION__, __LINE__);
    if (mInstance == NULL) {
        mInstance = new SubtitleServiceLinux();
    }

    return mInstance;
}

static pthread_once_t threadFlag = PTHREAD_ONCE_INIT;
SubtitleServiceLinux::SubtitleServiceLinux() {
    mIpcSocket = std::make_shared<IpcSocket>(true);
    mIpcSocket->start(SERVER_PORT, this, [](){
        pthread_once(&threadFlag, []{
            SubtitleSignalHandler::instance().installSignalHandlers();
        });
    });

    if (!mIpcSocket->isvalid()) {
        ALOGE("[%s] Server cocket init failed", __FUNCTION__ );
        mIpcSocket->stop();
        return;
    }

    mIpcSocket->setEventListener(eventReceiver);
    mpSubtitlecontrol = SubtitleServer::Instance();
}

/*static*/
int SubtitleServiceLinux::eventReceiver(int fd, void *selfData) {
    auto subService = static_cast<SubtitleServiceLinux*>(selfData);
    if (subService == nullptr) {
        return EventsTracker::RET_REMOVE;
    }

    char recvBuf[1024] = {0};
    int retLen = IpcSocket::receive(fd, recvBuf, sizeof(recvBuf));
    if (retLen <= 0) {
        ALOGD("Client broken, fd= %d, remove.", fd);
        subService->onRemoteDead(0);
        return EventsTracker::RET_REMOVE;
    }

    ALOGD("Receive: %s", recvBuf);
    int ret = subService->ParserSubtitleCommand(recvBuf, nullptr);

    std::string retStr = std::to_string(ret);
    bool send = IpcSocket::send(fd, retStr.c_str(), retStr.size());
    ALOGD("Reply %s", (send ? "OK" : "NG"));

    return EventsTracker::RET_CONTINUE;
}

SubtitleServiceLinux::~SubtitleServiceLinux() {
    ALOGD(" SubtitleServiceLinux  %s, line %d", __FUNCTION__, __LINE__);
    if (mpSubtitlecontrol != NULL) {
        delete mpSubtitlecontrol;
        mpSubtitlecontrol = NULL;
    }
}

int SubtitleServiceLinux::SplitCommand(const char *commandData) {
    char cmdbuff[1024];
    char *token;
    int cmd_size = 0;
    const char *delimitation = ".";
    strcpy(cmdbuff, commandData);

    /* get first str*/
    token = strtok(cmdbuff, delimitation);

    /* continue get str*/
    while (token != NULL) {
        mSubtitleCommand[cmd_size].assign(token);

        //ALOGE("%s mSubtitleCommand[%d]:%s\n", token, cmd_size, mSubtitleCommand[cmd_size].c_str());
        cmd_size++;
        token = strtok(NULL, delimitation);
    }
    //LOGD("%s: cmd_size = %d\n", __FUNCTION__, cmd_size);

    return cmd_size;
}

int SubtitleServiceLinux::SetTeleCmd(subtitle_module_param_t param) {
    int ret = 0;
    int moduleId = param.moduleId;
    if ((moduleId >= SUBTITLE_MODULE_CMD_START) && (moduleId <= SUBTITLE_CMD_MAX)) {
        int paramData[32] = {0};
        int i = 0;
        for (i = 0; i < param.paramLength; i++) {
            paramData[i] = param.paramBuf[i];
        }
        switch (moduleId) {
            case SUBTITLE_TTCONTROL:
                mpSubtitlecontrol->ttControl(paramData[0], paramData[1],
                                             paramData[2], paramData[3], paramData[4],
                                             paramData[5]);
                break;
            case SUBTITLE_TTGOHOME:
                mpSubtitlecontrol->ttGoHome(paramData[0]);
                break;
            case SUBTITLE_TTNEXTPAGE:
                mpSubtitlecontrol->ttNextPage(paramData[0], paramData[1]);
                break;
            case SUBTITLE_TTNEXTSUBPAGE:
                mpSubtitlecontrol->ttNextSubPage(paramData[0], paramData[1]);
                break;
            case SUBTITLE_TTGOTOPAGE:
                mpSubtitlecontrol->ttGotoPage(paramData[0],
                                              paramData[1], paramData[2]);
                break;
            default:
                break;
        }
    } else {
        ALOGE("%s: invalid Subtitle cmd: %d\n", __FUNCTION__, moduleId);
        ret = -1;
    }
    return ret;
}

int SubtitleServiceLinux::SetCmd(subtitle_module_param_t param, native_handle_t *handle) {
    int ret = 0;
    int moduleId = param.moduleId;
    ALOGD(" SubtitleServiceLinux.cpp %s line %d moduleId = %d\n", __FUNCTION__, __LINE__,
          moduleId);
    if ((moduleId >= SUBTITLE_MODULE_CMD_START) && (moduleId <= SUBTITLE_CMD_MAX)) {
        int paramData[32] = {0};
        int i = 0;
        for (i = 0; i < param.paramLength; i++) {
            paramData[i] = param.paramBuf[i];
        }
        ALOGI("in %s, moduleId = %d,paramData[0]=%d, paramData[1]=%d", __FUNCTION__,
              moduleId, paramData[0], paramData[1]);
        switch (moduleId) {
            case SUBTITLE_OPENCONNECTION:
                ret = mpSubtitlecontrol->openConnection();
                break;
            case SUBTITLE_CLOSECONNECTION:
                ret = mpSubtitlecontrol->closeConnection(paramData[0]);
                break;
            case SUBTITLE_OPEN:
                ret = mpSubtitlecontrol->open(paramData[0], paramData[1],
                                              (OpenType) paramData[2], handle);
                break;
            case SUBTITLE_CLOSE:
                ALOGD(" SubtitleServiceLinux  %s, line %d SUBTITLE_CLOSE", __FUNCTION__,
                      __LINE__);
                ret = mpSubtitlecontrol->close(paramData[0]);
                break;
            case SUBTITLE_RESETFORSEEK:
                ALOGD(" SubtitleServiceLinux  %s, line %d SUBTITLE_RESETFORSEEK",
                      __FUNCTION__, __LINE__);
                break;
            case SUBTITLE_UPDATEVIDEOPTS:
                ALOGD(" SubtitleServiceLinux  %s, line %d SUBTITLE_UPDATEVIDEOPTS",
                      __FUNCTION__, __LINE__);
                break;
            case SUBTITLE_SETPAGEID:
                ALOGD(" SubtitleServiceLinux  %s, line %d SUBTITLE_SETPAGEID", __FUNCTION__,
                      __LINE__);
                ret = mpSubtitlecontrol->setPageId(paramData[0], paramData[1]);
                break;
            case SUBTITLE_SETANCPAGEID:
                ALOGD(" SubtitleServiceLinux  %s, line %d", __FUNCTION__, __LINE__);
                ret = mpSubtitlecontrol->setAncPageId(paramData[0], paramData[1]);
                break;
            case SBUTITLE_SETCHANNELID:
                ALOGD(" SubtitleServiceLinux  %s, line %d SBUTITLE_SETCHANNELID",
                      __FUNCTION__, __LINE__);
                ret = mpSubtitlecontrol->setChannelId(paramData[0], paramData[1]);
                break;
            case SUBTITLE_SETCLOSECAPTIONVFMT:
                ALOGD(" SubtitleServiceLinux  %s, line %d", __FUNCTION__, __LINE__);
                break;
            case SUBTITLE_USERDATAOPEN:
                ALOGD(" SubtitleServiceLinux  %s, line %d", __FUNCTION__, __LINE__);
                ret = mpSubtitlecontrol->userDataOpen(paramData[0]);
                break;
            case SUBTITLE_USERDATACLOSE:
                ALOGD(" SubtitleServiceLinux  %s, line %d", __FUNCTION__, __LINE__);
                ret = mpSubtitlecontrol->userDataClose(paramData[0]);
                break;
            case SUBTITLE_SETSUBTYPE:
                ret = mpSubtitlecontrol->setSubType(paramData[0], paramData[1]);
                break;
            case SUBTITLE_SETRENDERTYPE:
                ret = mpSubtitlecontrol->setRenderType(paramData[0], paramData[1]);
                break;
            case SUBTITLE_SETPIPID:
                ret = mpSubtitlecontrol->setPipId(paramData[0], paramData[1], paramData[2]);
                break;
            case SUBTITLE_GETSUBPID:
                ret = mpSubtitlecontrol->setSubPid(paramData[0], paramData[1]);
                break;
            default:
                ALOGE("Can not be recognized moduleId: %d", moduleId);
                break;
        }
    } else {
        ALOGE("%s: invalid Subtitle cmd: %d\n", __FUNCTION__, moduleId);
        ret = -1;
    }

    return ret;
}

void SubtitleServiceLinux::join() {
    if (mIpcSocket != nullptr) {
        mIpcSocket->join();
    }
}

int SubtitleServiceLinux::GetCmd(subtitle_module_param_t param) {
    int ret = 0;
    int moduleId = param.moduleId;

    if ((moduleId >= SUBTITLE_MODULE_CMD_START) && (moduleId <= SUBTITLE_CMD_MAX)) {
        //int paramData[32] = {0};
        // int i = 0;
        //source_input_param_t source_input_param;

        //for (i = 0; i < param.paramLength; i++) {
        //    paramData[i] = param.paramBuf[i];
        //}

        switch (moduleId) {
            case SUBTITLE_GETTYPE:
                //ret = mpSubtitlecontrol->GetHue();
                break;
            case SUBTITLE_GETTOTALTRACKS:
                //ret = mpSubtitlecontrol->GetGrayPattern();
                break;
            case SUBTITLE_GETLANGUAGE:
                break;
            case SUBTITLE_GETSUBTYPE:
                break;
            case SUBTITLE_GETSUBPID:
                break;
            case SUBTITLE_GETANCPAGE:
                break;
            default:
                break;
        }
    } else {
        ALOGE("%s: invalid SUBTITLE cmd: %d!\n", __FUNCTION__, moduleId);
        ret = -1;
    }

    return ret;
}

void SubtitleServiceLinux::onRemoteDead(int sessionId) {
    if (mpSubtitlecontrol && !mpSubtitlecontrol->isClosed(sessionId)) {
        if (mpSubtitlecontrol->close(sessionId) == Result::OK
            && mpSubtitlecontrol->closeConnection(sessionId) == Result::OK) {
            ALOGD("onRemoteDead, self close OK for id %d", sessionId);
        } else {
            ALOGD("onRemoteDead, self close failed for id %d", sessionId);
        }
    } else {
        ALOGD("onRemoteDead, already closed for id %d", sessionId);
    }
}

int SubtitleServiceLinux::ParserSubtitleCommand(const char *commandData, native_handle_t *handle) {
    ALOGD(" SubtitleServiceLinux %s: cmd data is %s\n", __FUNCTION__, commandData);

    int cmd_size = 0;
    int ret = 0;
    int i = 0;
    //split command
    cmd_size = SplitCommand(commandData);
    //parse command
    subtitle_module_param_t subtitleParam;
    memset(&subtitleParam, 0, sizeof(subtitle_module_param_t));
    subtitleParam.moduleId = atoi(mSubtitleCommand[2].c_str());
    subtitleParam.paramLength = cmd_size - 3;
    for (i = 0; i < subtitleParam.paramLength; i++) {
        subtitleParam.paramBuf[i] = atoi(mSubtitleCommand[i + 3].c_str());
    }
    ALOGD(" SubtitleServiceLinux %s: cmd cmmand0 is %s, command1 is %s\n", __FUNCTION__,
          mSubtitleCommand[0].c_str(), mSubtitleCommand[1].c_str());
    if (strcmp(mSubtitleCommand[0].c_str(), "subtitle") == 0) {
        if ((strcmp(mSubtitleCommand[1].c_str(), "ctrl") == 0) ||
            (strcmp(mSubtitleCommand[1].c_str(), "set") == 0) ||
            (strcmp(mSubtitleCommand[1].c_str(), "afdcontrol") == 0)) {
            ret = SetCmd(subtitleParam, handle);
        } else if (strcmp(mSubtitleCommand[1].c_str(), "get") == 0) {
            ret = GetCmd(subtitleParam);
        } else if (strcmp(mSubtitleCommand[1].c_str(), "ttcontrol") == 0) {
            ret = SetTeleCmd(subtitleParam);
        } else if (strcmp(mSubtitleCommand[1].c_str(), "afdctrl") == 0) {
            ret = GetCmd(subtitleParam);
        } else {
            ALOGD("%s: invalid cmd\n", __FUNCTION__);
            ret = 0;
        }
    } else {
        ALOGD("%s: invalie cmdType\n", __FUNCTION__);
    }

    return ret;
}
#ifdef __cplusplus
}
#endif
