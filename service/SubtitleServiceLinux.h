/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: header file
 */

#ifndef SUBTITLE_SERVICE_LINUX_H
#define SUBTILE_SERVICE_LINUX_H

#include "SubtitleServer.h"

using namespace android;

class IpcSocket;

typedef struct subtitle_module_param_s {
    int moduleId;        //moduleId according to subtitlecmd.h
    int paramLength;     //length of parambuf
    int paramBuf[10];    //param for action
} subtitle_module_param_t;

class SubtitleServiceLinux {
public:
    SubtitleServiceLinux();
    ~SubtitleServiceLinux();
    static SubtitleServiceLinux *GetInstance();

    enum command {
        CMD_START = IBinder::FIRST_CALL_TRANSACTION,
        CMD_SUBTITLE_ACTION = IBinder::FIRST_CALL_TRANSACTION + 1,
    };

    void join();
    void onRemoteDead(int sessionId);
    int ParserSubtitleCommand(const char *commandData, native_handle_t* handle = nullptr);
    int SplitCommand(const char *commandData);
    int SetCmd(subtitle_module_param_t param, native_handle_t* handle);
    int SetTeleCmd(subtitle_module_param_t param);
    int GetCmd(subtitle_module_param_t param);

    static int eventReceiver(int fd, void *selfData);

    sp<IBinder> evtCallBack;
    SubtitleServer *mpSubtitlecontrol;
    std::string mSubtitleCommand[10];

    std::shared_ptr<IpcSocket> mIpcSocket;
};
#endif
