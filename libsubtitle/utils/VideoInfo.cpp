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

#define LOG_TAG "SysfsVideoInfo"

#include <unistd.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "SubtitleLog.h"

#include "VideoInfo.h"
#include "ParserFactory.h"

#define VIDEO_VDEC_CORE "/sys/class/vdec/core"
#define DEFAULT_VIDEO_WIDTH 720;
#define DEFAULT_VIDEO_HEIGHT 480;


static inline unsigned long sysfsReadInt(const char *path, int base) {
    int fd;
    unsigned long val = 0;
    char bcmd[32];
    ::memset(bcmd, 0, 32);
    fd = ::open(path, O_RDONLY);
    if (fd >= 0) {
        int c = ::read(fd, bcmd, sizeof(bcmd));
        if (c > 0) val = strtoul(bcmd, NULL, base);
        ::close(fd);
    }
    return val;
}


class SysfsVideoInfo : public VideoInfo {
public:
    SysfsVideoInfo() {}
    virtual ~SysfsVideoInfo() {}
    virtual int getVideoWidth() {
        int ret = 0;
        ret = sysfsReadInt("/sys/class/video/frame_width", 10);
        if (ret == 0) {
            ret = DEFAULT_VIDEO_WIDTH;
        }
        return ret;
    }

    virtual int getVideoHeight() {
        int ret = 0;
        ret = sysfsReadInt("/sys/class/video/frame_height", 10);
        if (ret == 0) {
            ret = DEFAULT_VIDEO_HEIGHT;
        }
        return ret;
    }

    /**
     *   helper function to check is h264 or mpeg codec.
     *   CC decoder process differently  in these 2 codecs
    */
    virtual int getVideoFormat() {
        int retry = 100; // check 200ms for parser ready
        while (retry-- >= 0) {
            int fd = ::open(VIDEO_VDEC_CORE, O_RDONLY);
            int bytes = 0;
            if (fd >= 0) {
                uint8_t ubuf8[1025] = {0};
                memset(ubuf8, 0, 1025);
                bytes = read(fd, ubuf8, 1024);
                ubuf8[1024] = 0;
                SUBTITLE_LOGI("getVideoFormat ubuf8:%s", ubuf8);
                if (strstr((const char*)ubuf8, "amvdec_h264"/*"vdec.h264.00"*/)) {
                    SUBTITLE_LOGI("H264_CC_TYPE");
                    close(fd);
                    return H264_CC_TYPE;
                } else if (strstr((const char*)ubuf8, "amvdec_h265"/*"vdec.h265.00"*/)) {
                    SUBTITLE_LOGI("H265_CC_TYPE");
                    close(fd);
                    return H265_CC_TYPE;
                } else if (strstr((const char*)ubuf8, "amvdec_mpeg12"/*"vdec.mpeg12.00"*/)) {
                    SUBTITLE_LOGI("MPEG_CC_TYPE");
                    close(fd);
                    return MPEG_CC_TYPE;
                } else if (strstr((const char*)ubuf8, "VDEC_STATUS_CONNECTED")) {
                    SUBTITLE_LOGI("vdec has connect, no mpeg or h264 type, return!");
                    close(fd);
                    return INVALID_CC_TYPE;
                }

                close(fd);
            } // else {
                //SUBTITLE_LOGE("open error:%d,%s!!", errno,strerror(errno));
            // }
            usleep(20*1000);
        }

        return INVALID_CC_TYPE;
    }


};


VideoInfo *VideoInfo::mInstance = nullptr;

VideoInfo *VideoInfo::Instance() {
    if (mInstance == nullptr) {
        mInstance = new SysfsVideoInfo();
    }
    return mInstance;
}
