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

#include <binder/Binder.h>
#include <binder/Parcel.h>
#include "SubtitleServer.h"

using namespace android;

typedef struct subtitle_moudle_param_s {
    int moudleId;        //moudleId according to subtitlecmd.h
    int paramLength;     //length of parambuf
    int paramBuf[10];    //param for action
} subtitle_moudle_param_t;

class SubtitleServiceLinux: public BBinder{
public:
    SubtitleServiceLinux();
    ~SubtitleServiceLinux();
    static SubtitleServiceLinux *GetInstance();

    enum command {
        CMD_START = IBinder::FIRST_CALL_TRANSACTION,
        CMD_SUBTITLE_ACTION = IBinder::FIRST_CALL_TRANSACTION + 1,
    };

private:
    int ParserSubtitleCommand(const char *commandData, native_handle_t* handle = nullptr);
    int SplitCommand(const char *commandData);
    int SetCmd(subtitle_moudle_param_t param, native_handle_t* handle);
    int SetTeleCmd(subtitle_moudle_param_t param);
    int GetCmd(subtitle_moudle_param_t param);

    sp<IBinder> evtCallBack;

    SubtitleServer *mpSubtitlecontrol;
    virtual status_t onTransact(uint32_t code,
                                const Parcel& data, Parcel* reply,
                                uint32_t flags = 0);
    std::string mSubtitleCommand[10];
};
#endif
