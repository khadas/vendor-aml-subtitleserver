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
