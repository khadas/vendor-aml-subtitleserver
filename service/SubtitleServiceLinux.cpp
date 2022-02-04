/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#define LOG_MOUDLE_TAG "SUBTITLE"
#define LOG_CLASS_TAG "SubtitleServiceLinux"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <cutils/native_handle.h>

#ifdef RDK_AML_SUBTITLE_SOCKET
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#endif //RDK_AML_SUBTITLE_SOCKET

#include "SubtitleServiceLinux.h"
#include "subtitlecmd.h"
#include "SubtitleServer.h"

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

SubtitleServiceLinux::SubtitleServiceLinux() {
    #ifdef RDK_AML_SUBTITLE_SOCKET
    mExitRequested = false;
    #endif //RDK_AML_SUBTITLE_SOCKET
    mpSubtitlecontrol = SubtitleServer::Instance();
}

SubtitleServiceLinux::~SubtitleServiceLinux() {
    ALOGD(" SubtitleServiceLinux  %s, line %d", __FUNCTION__, __LINE__);
    if (mpSubtitlecontrol != NULL) {
        delete mpSubtitlecontrol;
        mpSubtitlecontrol = NULL;
    }
    #ifdef RDK_AML_SUBTITLE_SOCKET
    mExitRequested = true;
    mThread.join();
    #endif //RDK_AML_SUBTITLE_SOCKET
}


//TODO proof-of-concept solution for subtitles via socket
#ifdef RDK_AML_SUBTITLE_SOCKET
int SubtitleServiceLinux::SubtitleServiceHandleMessage()
{
    // Open cmd socket communication
    mThread = std::thread(&SubtitleServiceLinux::__threadLoopCmd, this);

    //while (1) {
    //    usleep(500*1000);
    //}
    return 0;
}
#endif //#ifdef RDK_AML_SUBTITLE_SOCKET

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

int SubtitleServiceLinux::SetTeleCmd(subtitle_moudle_param_t param) {
    int ret = 0;
    int moudleId = param.moudleId;
    if ((moudleId >= SUBTITLE_MOUDLE_CMD_START) && (moudleId <= SUBTITLE_CMD_MAX)) {
        int paramData[32] = {0};
        int i = 0;
        for (i = 0; i < param.paramLength; i++) {
            paramData[i] = param.paramBuf[i];
        }
        switch (moudleId) {
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
        ALOGE("%s: invalid Subtitle cmd: %d\n", __FUNCTION__, moudleId);
        ret = -1;
    }
    return ret;
}

int SubtitleServiceLinux::SetCmd(subtitle_moudle_param_t param, native_handle_t *handle) {
    int ret = 0;
    int moudleId = param.moudleId;
    ALOGD(" SubtitleServiceLinux.cpp %s line %d moduleId = %d\n", __FUNCTION__, __LINE__,
          moudleId);
    if ((moudleId >= SUBTITLE_MOUDLE_CMD_START) && (moudleId <= SUBTITLE_CMD_MAX)) {
        int paramData[32] = {0};
        int i = 0;
        for (i = 0; i < param.paramLength; i++) {
            paramData[i] = param.paramBuf[i];
        }
        ALOGI("in %s, moduleId = %d,paramData[0]=%d, paramData[1]=%d", __FUNCTION__,
              moudleId, paramData[0], paramData[1]);
        switch (moudleId) {
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
                break;
            case SUBTITLE_USERDATACLOSE:
                ALOGD(" SubtitleServiceLinux  %s, line %d", __FUNCTION__, __LINE__);
                break;
            case SUBTITLE_SETSUBTYPE:
                ret = mpSubtitlecontrol->setSubType(paramData[0], paramData[1]);
                break;
            case SUBTITLE_SETRENDERTYPE:
                ret = mpSubtitlecontrol->setRenderType(paramData[0], paramData[1]);
                break;
            case SUBTITLE_GETSUBPID:
                ret = mpSubtitlecontrol->setSubPid(paramData[0], paramData[1]);
                break;
            default:
                break;
        }
    } else {
        ALOGE("%s: invalid Subtitle cmd: %d\n", __FUNCTION__, moudleId);
        ret = -1;
    }

    return ret;
}
#ifdef RDK_AML_SUBTITLE_SOCKET
char* SubtitleServiceLinux::GetCmd(subtitle_moudle_param_t param) {
#else
int SubtitleServiceLinux::GetCmd(subtitle_moudle_param_t param) {
#endif

    int ret = 0;
    int moudleId = param.moudleId;

    if ((moudleId >= SUBTITLE_MOUDLE_CMD_START) && (moudleId <= SUBTITLE_CMD_MAX)) {
        //int paramData[32] = {0};
        // int i = 0;
        //source_input_param_t source_input_param;

        //for (i = 0; i < param.paramLength; i++) {
        //    paramData[i] = param.paramBuf[i];
        //}

        switch (moudleId) {
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
        ALOGE("%s: invalid SUBTITLE cmd: %d!\n", __FUNCTION__, moudleId);
        ret = -1;
    }

    #ifdef RDK_AML_SUBTITLE_SOCKET
    sprintf(mRetBuf, "%d", ret);
    return mRetBuf;
    #else
    return ret;
    #endif

}

#ifdef RDK_AML_SUBTITLE_SOCKET
void SubtitleServiceLinux::ParserSubtitleCommand(const char *commandData, native_handle_t *handle) {
#else
int SubtitleServiceLinux::ParserSubtitleCommand(const char *commandData, native_handle_t *handle) {
#endif //RDK_AML_SUBTITLE_SOCKET

    ALOGD(" SubtitleServiceLinux %s: cmd data is %s\n", __FUNCTION__, commandData);

    int cmd_size = 0;
    int ret = 0;
    int i = 0;
    #ifdef RDK_AML_SUBTITLE_SOCKET
    char* ret_char;
    #endif //RDK_AML_SUBTITLE_SOCKET
    //split command
    cmd_size = SplitCommand(commandData);
    //parse command
    subtitle_moudle_param_t subtitleParam;
    memset(&subtitleParam, 0, sizeof(subtitle_moudle_param_t));
    subtitleParam.moudleId = atoi(mSubtitleCommand[2].c_str());
    subtitleParam.paramLength = cmd_size - 3;
    for (i = 0; i < subtitleParam.paramLength; i++) {
        subtitleParam.paramBuf[i] = atoi(mSubtitleCommand[i + 3].c_str());
    }
    ALOGD(" SubtitleServiceLinux %s: cmd cmmand0 is %s, command1 is %s\n", __FUNCTION__,
          mSubtitleCommand[0].c_str(), mSubtitleCommand[1].c_str());

    #ifdef RDK_AML_SUBTITLE_SOCKET
    if (strcmp(mSubtitleCommand[0].c_str(), "subtitle") == 0) {
        if ((strcmp(mSubtitleCommand[1].c_str(), "ctrl") == 0) ||
            (strcmp(mSubtitleCommand[1].c_str(), "set") == 0)) {
            ret = SetCmd(subtitleParam, handle);
            sprintf(mRetBuf, "%d", ret);
        } else if (strcmp(mSubtitleCommand[1].c_str(), "get") == 0) {
            ret_char = GetCmd(subtitleParam);
            sprintf(mRetBuf, "%d", ret_char);
        } else if (strcmp(mSubtitleCommand[1].c_str(), "ttctrl") == 0) {
            ret = SetTeleCmd(subtitleParam);
            sprintf(mRetBuf, "%d", ret);
        } else if (strcmp(mSubtitleCommand[1].c_str(), "afdctrl") == 0) {
            ret_char = GetCmd(subtitleParam);
            sprintf(mRetBuf, "%d", ret_char);
        } else {
            ALOGD("%s: invalid cmd\n", __FUNCTION__);
            ret = 0;
        }
    #else
    if (strcmp(mSubtitleCommand[0].c_str(), "subtitle") == 0) {
        if ((strcmp(mSubtitleCommand[1].c_str(), "ctrl") == 0) ||
            (strcmp(mSubtitleCommand[1].c_str(), "set") == 0)) {
            ret = SetCmd(subtitleParam, handle);
        } else if (strcmp(mSubtitleCommand[1].c_str(), "get") == 0) {
            ret = GetCmd(subtitleParam);
        } else if (strcmp(mSubtitleCommand[1].c_str(), "ttctrl") == 0) {
            ret = SetTeleCmd(subtitleParam);
        } else if (strcmp(mSubtitleCommand[1].c_str(), "afdctrl") == 0) {
            ret = GetCmd(subtitleParam);
        } else {
            ALOGD("%s: invalid cmd\n", __FUNCTION__);
            ret = 0;
        }
    #endif // RDK_AML_SUBTITLE_SOCKET
    } else {
        ALOGD("%s: invalie cmdType\n", __FUNCTION__);
    }

    #ifndef RDK_AML_SUBTITLE_SOCKET
    return ret;
    #endif // RDK_AML_SUBTITLE_SOCKET
}

status_t SubtitleServiceLinux::onTransact(uint32_t code,
                                          const Parcel &data, Parcel *reply,
                                          uint32_t flags) {

#ifndef RDK_AML_SUBTITLE_SOCKET
    switch (code) {
        case CMD_SUBTITLE_ACTION: {
            const char *command = data.readCString();
            native_handle_t *handle = data.readNativeHandle();
            int32_t ret = ParserSubtitleCommand(command, handle);
            reply->writeInt32(ret);
            break;
        }
        case SUBTITLE_SETFALLCALLBACK: {
            sp<ISubtitleCallback> client = interface_cast<ISubtitleCallback>(
                    data.readStrongBinder());
            mpSubtitlecontrol->setCallback(client, TYPE_HAL);
        }
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
#endif // RDK_AML_SUBTITLE_SOCKET
    return (0);
}

#ifdef RDK_AML_SUBTITLE_SOCKET
void SubtitleServiceLinux::__threadLoopCmd() {
    while (!mExitRequested && threadLoopCmd());
}

bool SubtitleServiceLinux::threadLoopCmd() {

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(LISTEN_PORT_CMD);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int flag = 1;

    int sockFd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockFd < 0) return true;

    if ((setsockopt(sockFd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag))) < 0) {
        LOGE("setsockopt failed.\n");
        close(sockFd);
        return true;
    }

    if (bind(sockFd, (struct sockaddr *)&addr,sizeof(addr)) == -1) {
        LOGE("bind fail. error=%d, err:%s\n", errno, strerror(errno));
        close(sockFd);
        return true;
    }

    if (listen(sockFd, QUEUE_SIZE) == -1) {
        LOGE("listen fail.error=%d, err:%s\n", errno, strerror(errno));
        close(sockFd);
        return true;
    }

    while (!mExitRequested) {
        struct sockaddr_in client_addr;
        socklen_t length = sizeof(client_addr);
        int connFd = accept(sockFd, (struct sockaddr*)&client_addr, &length);
        if (connFd < 0) {
            printf("client connect fail.error=%d, err:%s\n", errno, strerror(errno));
            close(sockFd);
            return true;
        }

        if (mClientThreads.size() >= QUEUE_SIZE) {
            mClientThreads.pop_front();
        }

        std::shared_ptr<std::thread> pthread = std::shared_ptr<std::thread>(
                new std::thread(&SubtitleServiceLinux::clientConnectedCmd, this, connFd));

        pthread->detach();
        mClientThreads.push_back(pthread);
    }

    printf("closed.\n");
    close(sockFd);
    return true;
}


int SubtitleServiceLinux::clientConnectedCmd(int sockfd)
{
    char recvBuf[1024] = {0};
    bool pendingByData = false;
    bool childConnected = false;
    IpcPackageHeader curHeader;

    while (!mExitRequested) {

        int retLen = recv(sockfd, recvBuf, sizeof(recvBuf), 0);
        if (errno != 0 && errno != EACCES) {
            LOGD("error no:%d %s", errno, strerror(errno));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (errno == EPIPE)
        {
            //exit thread in case of broken pipe
            break;
        }

        ParserSubtitleCommand(recvBuf);
        retLen = send(sockfd, mRetBuf, sizeof(mRetBuf), 0);
        if (retLen < 0) {
            if (errno == EINTR) {
                retLen = 0;
            }
        }
    }
    close(sockfd);
    return 0;
}
#endif // RDK_AML_SUBTITLE_SOCKET

#ifdef __cplusplus
}
#endif
