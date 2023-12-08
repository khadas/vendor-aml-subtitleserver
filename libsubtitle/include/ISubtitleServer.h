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
    virtual Result updateVideoPos(int32_t sId, int64_t pos) = 0;


    virtual void getTotalTracks(int32_t sId, getTotalTracks_cb _hidl_cb) = 0;
    virtual void getType(int32_t sId, getType_cb _hidl_cb) = 0;
    virtual void getLanguage(int32_t sId, getLanguage_cb _hidl_cb) = 0;

    virtual Result setSubType(int32_t sId, int32_t type) = 0;
    virtual Result setSubPid(int32_t sId, int32_t pid) = 0;
    virtual Result setPageId(int32_t sId, int32_t pageId) = 0;
    virtual Result setAncPageId(int32_t sId, int32_t ancPageId) = 0;
    virtual Result setChannelId(int32_t sId, int32_t channelId) = 0;
    virtual Result setClosedCaptionVfmt(int32_t sId, int32_t vfmt) = 0;

    virtual Result ttControl(int32_t sId, int cmd, int magazine, int pageNo, int regionId, int param) = 0;
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



