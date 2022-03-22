/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "ISubtitlelService"

#include <stdint.h>
#include <sys/types.h>

#include <binder/Parcel.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <ISubtitleServer.h>
#include <utils/Log.h>

namespace android {

class BpSubtitleServer : public BpInterface<ISubtitleServer>
{
public:
    BpSubtitleServer(const sp<IBinder>& impl)
        : BpInterface<ISubtitleServer>(impl) {
    }
    // Methods from ISubtitleServer follow.
    virtual void openConnection(openConnection_cb _hidl_cb) {
	   return;
    }

    virtual Result closeConnection(int32_t sId) {
	   return OK;
    }
    virtual Result open(int32_t sId, const hidl_handle& handle, int32_t ioType, OpenType openType) {
	   return OK;
    }
    virtual Result close(int32_t sId) {
	   return OK;
    }
    virtual Result resetForSeek(int32_t sId) {
	   return OK;
    }
    virtual Result updateVideoPos(int32_t sId, int32_t pos) {
	   return OK;
    }


    virtual void getTotalTracks(int32_t sId, getTotalTracks_cb _hidl_cb)  {
	   return;
    }
    virtual void getType(int32_t sId, getType_cb _hidl_cb)  {
	   return;
    }
    virtual void getLanguage(int32_t sId, getLanguage_cb _hidl_cb)  {
	   return;
    }

    virtual Result setSubType(int32_t sId, int32_t type) {
	   return OK;
    }
    virtual Result setSubPid(int32_t sId, int32_t pid) {
	   return OK;
    }
    virtual Result setPageId(int32_t sId, int32_t pageId) {
	   return OK;
    }
    virtual Result setAncPageId(int32_t sId, int32_t ancPageId) {
	   return OK;
    }
    virtual Result setChannelId(int32_t sId, int32_t channelId) {
	   return OK;
    }
    virtual Result setClosedCaptionVfmt(int32_t sId, int32_t vfmt){
	   return OK;
    }

    virtual Result ttControl(int32_t sId, int cmd, int magazine, int page, int regionId, int param) {
	   return OK;
    }
    virtual Result ttGoHome(int32_t sId) {
	   return OK;
    }
    virtual Result ttNextPage(int32_t sId, int32_t dir) {
	   return OK;
    }
    virtual Result ttNextSubPage(int32_t sId, int32_t dir) {
	   return OK;
    }
    virtual Result ttGotoPage(int32_t sId, int32_t pageNo, int32_t subPageNo) {
	   return OK;
    }

    virtual Result userDataOpen(int32_t sId) {
	   return OK;
    }
    virtual Result userDataClose(int32_t sId) {
	   return OK;
    }


    virtual void prepareWritingQueue(int32_t sId, int32_t size, prepareWritingQueue_cb _hidl_cb)  {
	   return;
    }

    virtual void setCallback(const sp<ISubtitleCallback>& callback, ConnectType type)  {
	   return;
    }
    virtual void setFallbackCallback(const sp<ISubtitleCallback>& callback, ConnectType type)  {
	   return;
    }
    virtual void removeCallback(const sp<ISubtitleCallback>& callback)  {
	   return;
    }

/*
    virtual int startScreenCap(int32_t left, int32_t top, int32_t right, int32_t bottom, int32_t width, int32_t height, int32_t sourceType, const char* fileName) {
        ALOGI("BpScreenControlService startScreenCap left:%d, top:%d, right:%d, bottom:%d, width:%d, height:%d, sourceType:%d, fileName:%s\n", left, top, right, bottom, width, height, sourceType, fileName);
        Parcel data, reply;
        data.writeInterfaceToken(IScreenControlService::getInterfaceDescriptor());
        data.writeInt32(left);
        data.writeInt32(top);
        data.writeInt32(right);
        data.writeInt32(bottom);
        data.writeInt32(width);
        data.writeInt32(height);
        data.writeInt32(sourceType);
        data.writeCString(fileName);
        remote()->transact(BnScreenControlService::SCREEN_CAP, data, &reply);
        return NO_ERROR;
    }

    virtual int startScreenRecord(int32_t width, int32_t height, int32_t frameRate, int32_t bitRate, int32_t limitTimeSec, int32_t sourceType, const char* fileName) {
        ALOGI("BpScreenControlService startScreenRecord width:%d, height:%d, frameRate:%d, bitRate:%d, limitTimeSec:%d, sourceType:%d, fileName\n", width, height, frameRate, bitRate, limitTimeSec, sourceType, fileName);
        Parcel data, reply;
        data.writeInterfaceToken(IScreenControlService::getInterfaceDescriptor());
        data.writeInt32(width);
        data.writeInt32(height);
        data.writeInt32(frameRate);
        data.writeInt32(bitRate);
        data.writeInt32(limitTimeSec);
        data.writeInt32(sourceType);
        data.writeCString(fileName);
        remote()->transact(BnScreenControlService::SCREEN_REC, data, &reply);
        return NO_ERROR;
    }
*/

};

IMPLEMENT_META_INTERFACE(SubtitleServer, "droidlogic.ISubtitleServer");

status_t BnScreenControlService::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    switch (code) {
        case SUBTITLE_OPENCONNECTION: {
      /*      CHECK_INTERFACE(IScreenControlService, data, reply);
            int32_t left = data.readInt32();
            int32_t top = data.readInt32();
            int32_t right = data.readInt32();
            int32_t bottom = data.readInt32();
            int32_t width = data.readInt32();
            int32_t height = data.readInt32();
            int32_t sourceType = data.readInt32();
            const char* fileName = data.readCString();
            ALOGI("BnScreenControlService onTransact left:%d, top:%d, right:%d, bottom:%d, width:%d, height:%d, sourceType:%d, fileName:%s\n", left, top, right, bottom, width, height, sourceType, fileName);
            int err = startScreenCap(left, top, right, bottom, width, height, sourceType, fileName);
            reply->writeInt32(err);*/
            return NO_ERROR;
        }

        case SUBTITLE_OPENCONNECTION: {
      /*      CHECK_INTERFACE(IScreenControlService, data, reply);
            int32_t width = data.readInt32();
            int32_t height = data.readInt32();
            int32_t frameRate = data.readInt32();
            int32_t bitRate = data.readInt32();
            int32_t limitTimeSec = data.readInt32();
            int32_t sourceType = data.readInt32();
            const char* fileName = data.readCString();
            ALOGI("BnScreenControlService onTransact width:%d, height:%d, frameRate:%d, bitRate:%d, limitTimeSec:%d, sourceType:%d, fileName:%s\n", width, height, frameRate, bitRate, limitTimeSec, sourceType, fileName);
            int err = startScreenRecord(width, height, frameRate, bitRate, limitTimeSec, sourceType, fileName);
            reply->writeInt32(err);*/
            return NO_ERROR;
        }

        default: {
            return BBinder::onTransact(code, data, reply, flags);
        }
    }
}

};