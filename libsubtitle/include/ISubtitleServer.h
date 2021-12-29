/* Copyright 2017 The Android Open Source Project
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

#pragma once

#include <list>
#include <binder/Binder.h>
#include <binder/Parcel.h>
#include <binder/IInterface.h>
#include <utils/Errors.h>
#include <utils/String8.h>
#include <utils/String16.h>

using android::IBinder;
using android::BBinder;
using android::IInterface;
using android::BpInterface;
using android::BnInterface;
using android::Parcel;
using android::sp;


class ISubtitleServer : public IInterface{

    // Methods from ISubtitleServer follow.
    virtual void openConnection(openConnection_cb _hidl_cb)  = 0;
    virtual Result closeConnection(int32_t sId) = 0;
    virtual Result open(int32_t sId, const hidl_handle& handle, int32_t ioType, OpenType openType) = 0;
    virtual Result close(int32_t sId) = 0;
    virtual Result resetForSeek(int32_t sId) = 0;
    virtual Result updateVideoPos(int32_t sId, int32_t pos) = 0;


    virtual void getTotalTracks(int32_t sId, getTotalTracks_cb _hidl_cb) = 0;
    virtual void getType(int32_t sId, getType_cb _hidl_cb) = 0;
    virtual void getLanguage(int32_t sId, getLanguage_cb _hidl_cb) = 0;

    virtual Result setSubType(int32_t sId, int32_t type) = 0;
    virtual Result setSubPid(int32_t sId, int32_t pid) = 0;
    virtual Result setPageId(int32_t sId, int32_t pageId) = 0;
    virtual Result setAncPageId(int32_t sId, int32_t ancPageId) = 0;
    virtual Result setChannelId(int32_t sId, int32_t channelId) = 0;
    virtual Result setClosedCaptionVfmt(int32_t sId, int32_t vfmt) = 0;

    virtual Result ttControl(int32_t sId, int cmd, int magazine, int page, int regionId, int param) = 0;
    virtual Result ttGoHome(int32_t sId) = 0;
    virtual Result ttNextPage(int32_t sId, int32_t dir) = 0;
    virtual Result ttNextSubPage(int32_t sId, int32_t dir) = 0;
    virtual Result ttGotoPage(int32_t sId, int32_t pageNo, int32_t subPageNo) = 0;

    virtual Result userDataOpen(int32_t sId) = 0;
    virtual Result userDataClose(int32_t sId) = 0;


    virtual void prepareWritingQueue(int32_t sId, int32_t size, prepareWritingQueue_cb _hidl_cb) = 0;

    virtual void setCallback(const sp<ISubtitleCallback>& callback, ConnectType type) = 0;
    virtual void setFallbackCallback(const sp<ISubtitleCallback>& callback, ConnectType type) = 0;
    virtual void removeCallback(const sp<ISubtitleCallback>& callback) = 0;
/*
    //============ for stand alone UI, proxy API =============
    virtual Result show(int32_t sId) = 0;
    virtual Result hide(int32_t sId) = 0;
    virtual Result setTextColor(int32_t sId, int32_t color) = 0;
    virtual Result setTextSize(int32_t sId, int32_t size) = 0;
    virtual Result setGravity(int32_t sId, int32_t gravity) = 0;
    virtual Result setTextStyle(int32_t sId, int32_t style) = 0;
    virtual Result setPosHeight(int32_t sId, int32_t height) = 0;
    virtual Result setImgRatio(int32_t sId, float ratioW, float ratioH, int32_t maxW, int32_t maxH) = 0;
    virtual void getSubDemision(int32_t sId, getSubDemision_cb _hidl_cb) = 0;
    virtual Result setSubDemision(int32_t sId, int32_t width, int32_t height) = 0;
    virtual Result setSurfaceViewRect(int32_t sId, int32_t x, int32_t y, int32_t w, int32_t h) = 0;
    virtual Result setPipId(int32_t sId, int32_t mode, int32_t id) = 0;
*/

};



