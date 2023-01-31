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

#define LOG_TAG "DemuxDeviceSource"

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include "sub_types2.h"

#include <string>
//#ifdef ANDROID
#include <utils/Log.h>
#include <utils/CallStack.h>
//#endif
//#include "trace_support.h"
#include "DemuxSource.h"
#include "ParserFactory.h"

#include <pthread.h>

#ifdef MEDIASYNC_FOR_SUBTITLE
extern "C"  {
#include "MediaSyncInterface.h"
mediasync_result MediaSync_bindInstance(void* handle, uint32_t SyncInsId,
                                                             sync_stream_type streamtype);
mediasync_result MediaSync_getTrackMediaTime(void* handle, int64_t *outMediaUs);

}
#endif

static const std::string SYSFS_VIDEO_PTS = "/sys/class/tsync/pts_video";
static const std::string SYSFS_VIDEO_FIRSTPTS = "/sys/class/tsync/firstvpts";
static const std::string SYSFS_VIDEO_FIRSTRRAME = "/sys/module/amvideo/parameters/first_frame_toggled";
static const std::string SYSFS_SUBTITLE_TYPE = "/sys/class/subtitle/subtype";
static const std::string SYSFS_SUBTITLE_SIZE = "/sys/class/subtitle/size";
static const std::string SYSFS_SUBTITLE_TOTAL = "/sys/class/subtitle/total";
static const std::string SUBTITLE_READ_DEVICE = "/dev/amstream_sub_read";

#define AMSTREAM_IOC_MAGIC  'S'
#define AMSTREAM_IOC_SUB_LENGTH  _IOR(AMSTREAM_IOC_MAGIC, 0x1b, int)
#define AMSTREAM_IOC_SUB_RESET   _IOW(AMSTREAM_IOC_MAGIC, 0x1a, int)
#define SECTION_DEMUX_INDEX 2
//#define DUMP_SUB_DATA
DemuxSource *DemuxSource::sInstance = nullptr;
DemuxSource *DemuxSource::getCurrentInstance() {
    return DemuxSource::sInstance;
}



static inline unsigned long sysfsReadInt(const char *path, int base) {
    int fd;
    unsigned long val = 0;
    char bcmd[32];
    memset(bcmd, 0, 32);
    fd = ::open(path, O_RDONLY);
    if (fd >= 0) {
        int c = read(fd, bcmd, sizeof(bcmd));
        if (c > 0) {
            val = strtoul(bcmd, NULL, base);
        }
        ::close(fd);
    } else {
        ALOGE("unable to open file %s,err: %s", path, strerror(errno));
    }
    return val;
}

static void close_dvb_dmx(TVSubtitleData *data, int dmx_id) {
    if (data->filter_handle != -1) {
         ALOGE("[close_dvb_dmx]AM_DMX_StopFilter");
         AM_DMX_StopFilter(dmx_id, data->filter_handle);
    }
    if (data->filter_handle != -1) {
        ALOGE("[close_dvb_dmx]AM_DMX_FreeFilter");
        AM_DMX_FreeFilter(dmx_id, data->filter_handle);
    }
    if (data->filter_handle != -1) {
        ALOGE("[close_dvb_dmx]AM_DMX_SetCallback");
        AM_DMX_SetCallback(dmx_id, data->filter_handle, NULL, NULL);
    }
    if (data->dmx_id != -1) {
        ALOGE("[close_dvb_dmx]AM_DMX_Close");
        AM_DMX_Close(dmx_id);
    }
}

DemuxSource::DemuxSource() : mRdFd(-1), mState(E_SOURCE_INV),
        mExitRequested(false), mDemuxContext(nullptr), mParam1(-1), mParam2(-1) {
    mState = E_SOURCE_INV;
    mExitRequested = false;
    mNeedDumpSource = false;
    mDumpFd = -1;
    sInstance = this;
    mPlayerId = -1;
    mMediaSyncId = -1;
    subType = -1;
    #ifdef MEDIASYNC_FOR_SUBTITLE
    mMediaSync = MediaSync_create();
    #endif
    ALOGD("DeviceSource");
}

DemuxSource::~DemuxSource() {
    ALOGD("~DeviceSource");
    mExitRequested = true;
    if (mRenderTimeThread != nullptr) {
        mRenderTimeThread->join();
        mRenderTimeThread = nullptr;
    }

    if (mReadThread != nullptr) {
        mReadThread->join();
        mReadThread = nullptr;
    }

    #ifdef MEDIASYNC_FOR_SUBTITLE
    if (mMediaSync != nullptr) {
        MediaSync_destroy(mMediaSync);
    }
    #endif

    mPlayerId = -1;
    mMediaSyncId = -1;
    close_dvb_dmx(mDemuxContext, mDemuxId );
    if (mDumpFd > 0) ::close(mDumpFd);
}

size_t DemuxSource::totalSize() {
    return 0;
}


bool DemuxSource::notifyInfoChange() {
    std::unique_lock<std::mutex> autolock(mLock);
    for (auto it = mInfoListeners.begin(); it != mInfoListeners.end(); it++) {
        auto wk_lstner = (*it);
        int value = -1;

        if (auto lstn = wk_lstner.lock()) {
            lstn->onTypeChanged(subType);
            return true;
        }
    }

    return false;
}

void DemuxSource::loopRenderTime() {
    while (!mExitRequested) {
        mLock.lock();
        for (auto it = mInfoListeners.begin(); it != mInfoListeners.end(); it++) {
            auto wk_lstner = (*it);

            if (wk_lstner.expired()) {
                ALOGV("[threadLoop] lstn null.\n");
                continue;
            }

            ALOGD("mMediaSyncId= %d", mMediaSyncId);
            int64_t value;
            if (-1 == mMediaSyncId) {
                value = sysfsReadInt(SYSFS_VIDEO_PTS.c_str(), 16);
                mSyncPts = value;
            #ifdef MEDIASYNC_FOR_SUBTITLE
            } else {
                MediaSync_getTrackMediaTime(mMediaSync, &value);
                value = 0x1FFFFFFFF & ((9*value)/100);
                mSyncPts = value;
            #endif
            }
            static int i = 0;
            if (i++%300 == 0) {
                ALOGE(" read pts: %lld %llu", value, value);
            }
            if (!mExitRequested) {
                if (auto lstn = wk_lstner.lock()) {
                    lstn->onRenderTimeChanged(value);
                }
            }
        }
        mLock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

static void pes_data_cb(int dev_no, int fhandle, const uint8_t *data, int len, void *user_data)
{
    char *rdBuffer = new char[len]();
    memcpy(rdBuffer, data, len);
    std::shared_ptr<char> spBuf = std::shared_ptr<char>(rdBuffer, [](char *buf) { delete [] buf; });
    DemuxSource::getCurrentInstance()->mSegment->push(spBuf, len);
    #ifdef DUMP_SUB_DATA
    //for dump
    if (DemuxSource::getCurrentInstance()->mDumpFd == -1) {
        ALOGD("#pes_data_cb len:%d", len);
        DemuxSource::getCurrentInstance()->mDumpFd = ::open("/tmp/cur_sub.dump", O_RDWR | O_CREAT, 0666);
        ALOGD("need dump Source2: mDumpFd=%d %d", DemuxSource::getCurrentInstance()->mDumpFd, errno);
    }

    if (DemuxSource::getCurrentInstance()->mDumpFd > 0) {
        write(DemuxSource::getCurrentInstance()->mDumpFd, data, len);
    }
    #endif

}


static int open_dvb_dmx(TVSubtitleData *data, int dmx_id, int pid)
{
        ALOGE("[open_dmx]dmx_id:%d,pid:%d", dmx_id, pid);
        AM_DMX_OpenPara_t op;
        struct dmx_pes_filter_params pesp;
        struct dmx_sct_filter_params param;
        AM_ErrorCode_t ret;

        data->dmx_id = -1;
        data->filter_handle = -1;
        memset(&op, 0, sizeof(op));

        ret = AM_DMX_Open(dmx_id, &op);
        if (ret != AM_SUCCESS)
            goto error;
        data->dmx_id = dmx_id;
        ALOGE("[open_dmx]AM_DMX_Open");

        ret = AM_DMX_AllocateFilter(dmx_id, &data->filter_handle);
        if (ret != AM_SUCCESS)
            goto error;
        ALOGE("[open_dmx]AM_DMX_AllocateFilter");

        ret = AM_DMX_SetBufferSize(dmx_id, data->filter_handle, 0x80000);
        if (ret != AM_SUCCESS)
            goto error;
        ALOGE("[open_dmx]AM_DMX_SetBufferSize");
        memset(&pesp, 0, sizeof(pesp));
        pesp.pid = pid;
        pesp.output = DMX_OUT_TAP;
        if (12 == DemuxSource::getCurrentInstance()->subType) {
            ALOGE("[open_dmx] dvb demux");
            pesp.pes_type = DMX_PES_SUBTITLE;
            pesp.input = DMX_IN_FRONTEND;
            ret = AM_DMX_SetPesFilter(dmx_id, data->filter_handle, &pesp);
            if (ret != AM_SUCCESS)
                goto error;
            ALOGE("[open_dmx]AM_DMX_SetPesFilter");
        } else if (13 == DemuxSource::getCurrentInstance()->subType) {
            ALOGE("[open_dmx] teletext demux");
            pesp.pes_type = DMX_PES_SUBTITLE;//DMX_PES_TELETEXT;
            pesp.input = DMX_IN_FRONTEND;
            ret = AM_DMX_SetPesFilter(dmx_id, data->filter_handle, &pesp);
            if (ret != AM_SUCCESS)
                goto error;
            ALOGE("[open_dmx]AM_DMX_SetPesFilter");
        } else if (14 == DemuxSource::getCurrentInstance()->subType) {
            //the scte27 data is section
            ALOGE("[open_dmx] scte27 demux: start section filter");
            memset(&param, 0, sizeof(param));
            param.pid = pid;
            param.filter.filter[0] = SCTE27_TID;
            param.filter.mask[0] = 0xff;
            param.flags = DMX_CHECK_CRC;
            ret = AM_DMX_SetSecFilter(dmx_id, data->filter_handle, &param);
            if (ret != AM_SUCCESS) {
                goto error;
            }
        }

        ret = AM_DMX_SetCallback(dmx_id, data->filter_handle, pes_data_cb, data);
        if (ret != AM_SUCCESS)
            goto error;
        ALOGE("[open_dmx]AM_DMX_SetCallback");

        ret = AM_DMX_StartFilter(dmx_id, data->filter_handle);
        if (ret != AM_SUCCESS)
            goto error;
        ALOGE("[open_dmx]AM_DMX_StartFilter");

        return 0;
error:
          ALOGE("[open_dmx]ERROR !!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        if (data->filter_handle != -1) {
            AM_DMX_FreeFilter(dmx_id, data->filter_handle);
        }
        if (data->dmx_id != -1) {
            AM_DMX_Close(dmx_id);
        }

        return -1;
    }

    SubtitleIOType DemuxSource::type() {
        return E_SUBTITLE_DEMUX;
    }

bool DemuxSource::start() {
    if (E_SOURCE_STARTED == mState) {
        ALOGE("already stated");
        return false;
    }
    ALOGE(" DemuxSource start  mPid:%d", mPid);
    mState = E_SOURCE_STARTED;
    int total = 0;
    int subtype = 0;
    TVSubtitleData *mDvbContext;
    mSegment = std::shared_ptr<BufferSegment>(new BufferSegment());
    mDemuxContext = new TVSubtitleData();
    int ret = open_dvb_dmx(mDemuxContext, mDemuxId, mPid );
    if (ret == -1)
      return false;

    mRenderTimeThread = std::shared_ptr<std::thread>(new std::thread(&DemuxSource::loopRenderTime, this));

    notifyInfoChange();

    return false;
}

void DemuxSource::updateParameter(int type, void *data) {
   ALOGE(" in updateParameter type = %d ",type);
   bool restartDemux =false;
   if (12 == type) {
        DtvKitDvbParam *pDvbParam = (DtvKitDvbParam* )data;
        if ((mState == E_SOURCE_STARTED) && (mPid != pDvbParam->pid)) {
              restartDemux = true;
        }
        mDemuxId = pDvbParam->demuxId;
        mPid = pDvbParam->pid;
        mParam1 = pDvbParam->compositionId;
        mParam2 = pDvbParam->ancillaryId;
    } else if (13 == type) {
        TeletextParam *pTeleteParam = (TeletextParam* )data;
        if ((mState == E_SOURCE_STARTED) && (mPid != pTeleteParam->pid)) {
             restartDemux = true;
        }
        mDemuxId = pTeleteParam->demuxId;
        mPid = pTeleteParam->pid;
        mParam1 = pTeleteParam->magazine;
        mParam2 = pTeleteParam->page;
    } else if (14 == type) {
        Scte27Param *pScteParam = (Scte27Param* )data;
        if ((mState == E_SOURCE_STARTED) && (mPid != pScteParam->SCTE27_PID)) {
             restartDemux = true;
        }
        mDemuxId = pScteParam->demuxId;
        mPid = pScteParam->SCTE27_PID;
    }
    ALOGE(" in updateParameter restartDemux=%d ",restartDemux);
    if (restartDemux) {
        close_dvb_dmx(mDemuxContext, mDemuxId);
        open_dvb_dmx(mDemuxContext, mDemuxId, mPid );
     }
     subType = type;
    ALOGE(" updateParameter mPid:%d, demuxId = %d", mPid, mDemuxId);
    return;
}

void DemuxSource::setPipId (int mode, int id) {
   ALOGE("setPipId  mode:%d, id = %d", mode, id);
   if (PIP_PLAYER_ID == mode) {
       mPlayerId = id;
   } else if (PIP_MEDIASYNC_ID == mode) {
       if ((-1 != mMediaSyncId) || (mMediaSyncId != id)) {
           mMediaSyncId = id;
           if (mMediaSync != nullptr) {
               #ifdef MEDIASYNC_FOR_SUBTITLE
               MediaSync_destroy(mMediaSync);
               mMediaSync = MediaSync_create();
               #endif
           }
           #ifdef MEDIASYNC_FOR_SUBTITLE
           MediaSync_bindInstance(mMediaSync, mMediaSyncId, MEDIA_VIDEO);
           #endif
       }
   }
}

bool DemuxSource::stop() {
    mState = E_SOURCE_STOPED;

    // If parser has problem, it read more data than provided
    // then stop cannot stopped. need notify exit here.
    mSegment->notifyExit();
    close_dvb_dmx(mDemuxContext, DEMUX_SOURCE_ID);
    return false;
}

bool DemuxSource::isFileAvailble() {
    //unsigned long firstVpts = sysfsReadInt(SYSFS_VIDEO_FIRSTPTS.c_str(), 16);
    //unsigned long vPts = sysfsReadInt(SYSFS_VIDEO_PTS.c_str(), 16);
    unsigned long firstFrameToggle = sysfsReadInt(SYSFS_VIDEO_FIRSTRRAME.c_str(), 10);
    //ALOGD("%s firstFrame:%ld\n", __FUNCTION__, firstFrame);
    return firstFrameToggle == 1;
}

size_t DemuxSource::availableDataSize() {
    if (mSegment == nullptr) return 0;

    return mSegment->availableDataSize();
}

size_t DemuxSource::read(void *buffer, size_t size) {
    size_t readed = 0;

    //Current design of Parser Read, do not need add lock protection.
    // because all the read, is in Parser's parser thread.
    // We only need add lock here, is for protect access the mCurrentItem's
    // member function multi-thread.
    // Current Impl do not need lock, if required, then redesign the SegmentBuffer.

    //in case of applied size more than 1024*2, such as dvb subtitle,
    //and data process error for two reads.
    //so add until read applied data then exit.
    while (readed != size && mState == E_SOURCE_STARTED) {
        if (mCurrentItem != nullptr && !mCurrentItem->isEmpty()) {
            if (size == 0xffff)
            {
                //if size is 0xffff, it means to get all the data of the current item
                size = mCurrentItem->getSize();
            }
            readed += mCurrentItem->read_l(((char *)buffer+readed), size-readed);
            //ALOGD("readed:%d,size:%d", readed, size);
            if (readed == size) return readed;
        } else {
            //ALOGD("mCurrentItem null, pop next buffer item");
            mCurrentItem = mSegment->pop();
        }
    }
    //readed += mCurrentItem->read(((char *)buffer+readed), size-readed);
    return readed;

}


void DemuxSource::dump(int fd, const char *prefix) {
    dprintf(fd, "%s nDemuxSource:\n", prefix);
    {
        std::unique_lock<std::mutex> autolock(mLock);
        for (auto it = mInfoListeners.begin(); it != mInfoListeners.end(); it++) {
            auto wk_lstner = (*it);
            if (auto lstn = wk_lstner.lock())
                dprintf(fd, "%s   InforListener: %p\n", prefix, lstn.get());
        }
    }
    dprintf(fd, "%s   state:%d\n\n", prefix, mState);

    dprintf(fd, "%s   Total Subtitle Tracks: (%s)%ld\n", prefix,
            SYSFS_SUBTITLE_TOTAL.c_str(), sysfsReadInt(SYSFS_SUBTITLE_TOTAL.c_str(), 10));
    dprintf(fd, "%s   Current Subtitle type: (%s)%ld\n", prefix,
            SYSFS_SUBTITLE_TYPE.c_str(), sysfsReadInt(SYSFS_SUBTITLE_TYPE.c_str(), 10));

    dprintf(fd, "\n%s   Current Unconsumed Data Size: %d\n", prefix, availableDataSize());
}

