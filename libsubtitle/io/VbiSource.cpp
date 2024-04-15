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

/*
 *for teletext atv data source
 */

#define LOG_TAG "VbiSource"

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <string>
#include <pthread.h>

#include "SubtitleLog.h"
#include <utils/CallStack.h>

#include "VbiSource.h"

static const std::string VBI_DEV_FILE = "/dev/vbi";
static const std::string SYSFS_VIDEO_PTS = "/sys/class/tsync/pts_video";

#define DTV_SUB_DVB_TELETEXT 8
#define MAX_DUMP_DATA_LENGTH 3000

static inline bool pollFd(int fd, int timeout) {
    struct pollfd pollFd[1];
    if (fd <= 0)    {
        return false;
    }
    pollFd[0].fd = fd;
    pollFd[0].events = POLLIN;
    return (::poll(pollFd, 1, timeout) > 0);
}

static inline unsigned long sysfsReadInt(const char *path, int base) {
    int fd;
    unsigned long val = 0;
    char bcmd[32];
    memset(bcmd, 0, 32);
    fd = ::open(path, O_RDONLY);
    if (fd >= 0) {
        int c = read(fd, bcmd, sizeof(bcmd));
        if (c > 0) val = strtoul(bcmd, NULL, base);
        ::close(fd);
    } else {
        SUBTITLE_LOGE("unable to open file %s,err: %s", path, strerror(errno));
    }
    return val;
}

VbiSource::VbiSource() {
    mState = E_SOURCE_INV;
    int type = VBI_TYPE_TT_625B;
    mExitRequested = false;
    mNeedDumpSource = false;
    mDumpSub = false;
    checkDebug();
    mDumpFd = -1;
    mRdFd = ::open(VBI_DEV_FILE.c_str(), O_RDONLY);
    if (mRdFd == -1) {
        SUBTITLE_LOGI("cannot open %s", VBI_DEV_FILE.c_str());
        return;
    }
    if (::ioctl(mRdFd, VBI_IOC_SET_TYPE, &type )== -1)
        SUBTITLE_LOGI("VBI_IOC_SET_TYPE error:%s", strerror(errno));

    if (::ioctl(mRdFd, VBI_IOC_START) == -1)
        SUBTITLE_LOGI("VBI_IOC_START error:%s", strerror(errno));
    SUBTITLE_LOGI("VbiSource: %s=%d", VBI_DEV_FILE.c_str(), mRdFd);
}

VbiSource::~VbiSource() {
    SUBTITLE_LOGI("~VbiSource: %s=%d", VBI_DEV_FILE.c_str(), mRdFd);

    mExitRequested = true;
    if (mRenderTimeThread != nullptr) {
        mRenderTimeThread->join();
        mRenderTimeThread = nullptr;
    }

    ::close(mRdFd);
    if (mReadThread != nullptr) {
        mReadThread->join();
        mReadThread = nullptr;
    }

    if (mDumpFd > 0) ::close(mDumpFd);
}

size_t VbiSource::totalSize() {
    return 0;
}

void VbiSource::checkDebug() {
    #ifdef NEED_DUMP_ANDROID
    char value[PROPERTY_VALUE_MAX] = {0};
    memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("vendor.subtitle.dump", value, "false");
    if (!strcmp(value, "true")) {
        mDumpSub = true;
    }
    #endif
}

void VbiSource::updateParameter(int type, void *data) {
   SUBTITLE_LOGI(" in updateParameter type = %d ",type);
   return;
}

bool VbiSource::notifyInfoChange() {
    std::unique_lock<std::mutex> autolock(mLock);
    for (auto it = mInfoListeners.begin(); it != mInfoListeners.end(); it++) {
        auto wk_listener = (*it);
        int value = -1;


        if (auto lstn = wk_listener.lock()) {
            value = DTV_SUB_DVB_TELETEXT;
            if (value > 0) {  //0:no sub
                lstn->onTypeChanged(value);
            }

            value = 1;//sysfsReadInt(SYSFS_SUBTITLE_TOTAL.c_str(), 10);
            if (value >= 0) {
                lstn->onSubtitleChanged(value);
            }
            return true;
        }
    }

    return false;
}

void VbiSource::loopRenderTime() {
    while (!mExitRequested) {
        mLock.lock();
        for (auto it = mInfoListeners.begin(); it != mInfoListeners.end(); it++) {
            auto wk_listener = (*it);

            if (wk_listener.expired()) {
                SUBTITLE_LOGI("[threadLoop] lstn null.\n");
                continue;
            }

            unsigned long value = sysfsReadInt(SYSFS_VIDEO_PTS.c_str(), 16);

            static int i = 0;
            if (i++%300 == 0) {
                SUBTITLE_LOGI("read pts: %ld %lu", value, value);
            }
            if (!mExitRequested) {
                if (auto lstn = wk_listener.lock()) {
                    lstn->onRenderTimeChanged(value);
                }
            }
        }
        mLock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

void VbiSource::loopDriverData() {
    struct vbi_data_s vbi;

    while (!mExitRequested && mRdFd > 0) {
        int ret;
        if (pollFd(mRdFd, 15)) {
            struct vbi_data_s *pd;
            int ret;
            ret = ::read(mRdFd, &vbi, sizeof(vbi));
            pd = &vbi;

            if (ret >= (int)sizeof(struct vbi_data_s)) {
                //SUBTITLE_LOGI("loopDriverData=line:%d data:%s", pd->line_num, pd->b);
                unsigned char sub_header[ATV_TELETEXT_SUB_HEADER_LEN] = {0x41, 0x4d, 0x4c, 0x55, 0x41, 0};
                sub_header[5] = 0;//line_num 16 bit, for coverity
                sub_header[6] = 0;//line_num 16 bit, for coverity
                sub_header[7] = (pd->line_num >> 8) & 0xff;
                sub_header[8] = pd->line_num & 0xff;

                int size = ATV_TELETEXT_SUB_HEADER_LEN + 42;
                char *rdBuffer = new char[size]();
                //int read = readDriverData(rdBuffer,  size);
                memcpy(rdBuffer, sub_header, ATV_TELETEXT_SUB_HEADER_LEN);
                memcpy(rdBuffer + ATV_TELETEXT_SUB_HEADER_LEN, (char *)pd->b, 42);
                std::shared_ptr<char> spBuf = std::shared_ptr<char>(rdBuffer, [](char *buf) { delete [] buf; });
                mSegment->push(spBuf, size);
                if (mDumpSub) {
                    static char slice_buffer[MAX_DUMP_DATA_LENGTH * 4];
                    for (int i=0; i < strlen((char*)pd->b) && i< MAX_DUMP_DATA_LENGTH; i++) {
                        sprintf(&slice_buffer[i*3], " %02x", slice_buffer[i]);
                    }
                    SUBTITLE_LOGI("line data: %s", slice_buffer);
                }
            }
        }
    }
}

bool VbiSource::start() {
    if (E_SOURCE_STARTED == mState) {
        SUBTITLE_LOGE("already stated");
        return false;
    }
    mState = E_SOURCE_STARTED;
    int total = 0;
    int subtype = 0;


    mSegment = std::shared_ptr<BufferSegment>(new BufferSegment());

    /*if (total > 0 && subtype > 0) {
        mRenderTimeThread = std::shared_ptr<std::thread>(new std::thread(&DeviceSource::loopRenderTime, this));
    }*/

    mReadThread = std::shared_ptr<std::thread>(new std::thread(&VbiSource::loopDriverData, this));

    // first check types
    notifyInfoChange();

    return false;
}

bool VbiSource::stop() {
    mState = E_SOURCE_STOPPED;
    ::ioctl(mRdFd, VBI_IOC_STOP);

    // If parser has problem, it read more data than provided
    // then stop cannot stopped. need notify exit here.
    mSegment->notifyExit();

    return false;
}

SubtitleIOType VbiSource::type() {
    return E_SUBTITLE_VBI;
}

bool VbiSource::isFileAvailable() {
    return false;
}

size_t VbiSource::availableDataSize() {
    if (mSegment == nullptr) return 0;

    return mSegment->availableDataSize();
}

size_t VbiSource::readDriverData(void *buffer, size_t size) {
    int data_size = size, r, read_done = 0;
    char *buf = (char *)buffer;


    if (mNeedDumpSource) {
        if (mDumpFd == -1) {
            mDumpFd = ::open("/data/local/traces/cur_sub.dump", O_RDWR | O_CREAT, 0666);
            SUBTITLE_LOGI("need dump Source: mDumpFd=%d %d", mDumpFd, errno);
        }

        if (mDumpFd > 0) {
            write(mDumpFd, buffer, size);
        }
    }

    return read_done;
}

size_t VbiSource::read(void *buffer, size_t size) {
    int read = 0;

    //Current design of Parser Read, do not need add lock protection.
    // because all the read, is in Parser's parser thread.
    // We only need add lock here, is for protect access the mCurrentItem's
    // member function multi-thread.
    // Current Impl do not need lock, if required, then redesign the SegmentBuffer.

    //in case of applied size more than 1024*2, such as dvb subtitle,
    //and data process error for two reads.
    //so add until read applied data then exit.
    while (read != size && mState == E_SOURCE_STARTED) {
        if (mCurrentItem != nullptr && !mCurrentItem->isEmpty()) {
            read += mCurrentItem->read_l(((char *)buffer+read), size-read);
            //SUBTITLE_LOGI("read:%d,size:%d", read, size);
            if (read == size) return read;
        } else {
            //SUBTITLE_LOGI("mCurrentItem null, pop next buffer item");
            mCurrentItem = mSegment->pop();
        }
    }
    //read += mCurrentItem->read(((char *)buffer+read), size-read);
    return read;

}

void VbiSource::dump(int fd, const char *prefix) {
    dprintf(fd, "%s nVBISource:\n", prefix);
    {
        std::unique_lock<std::mutex> autolock(mLock);
        for (auto it = mInfoListeners.begin(); it != mInfoListeners.end(); it++) {
            auto wk_listener = (*it);
            if (auto lstn = wk_listener.lock())
                dprintf(fd, "%s   InfoListener: %p\n", prefix, lstn.get());
        }
    }
    dprintf(fd, "%s   state:%d\n\n", prefix, mState);
    dprintf(fd, "\n%s   Current Unconsumed Data Size: %d\n", prefix, availableDataSize());
}
