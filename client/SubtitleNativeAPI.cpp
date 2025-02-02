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

#define LOG_TAG "SubtitleNativeAPI"

#include "SubtitleLog.h"
#include <utils/CallStack.h>
#include <mutex>

#include "SubtitleClientLinux.h"
#include "SubtitleNativeAPI.h"

#define INVALID_PIP_ID -1

using ::android::CallStack;

enum RenderType {
    CALLBACK_RENDER = 0,
    DIRECT_RENDER = 1,
};

static AmlSubDataType __mapServerType2ApiType(int type) {
    switch (type) {
    case TYPE_SUBTITLE_DVB:
    case TYPE_SUBTITLE_PGS:
    case TYPE_SUBTITLE_SMPTE_TTML:
        return SUB_DATA_TYPE_POSITION_BITMAP;

    case TYPE_SUBTITLE_EXTERNAL:
    case TYPE_SUBTITLE_MKV_STR:
    case TYPE_SUBTITLE_SSA:
    case TYPE_SUBTITLE_ARIB_B24:
    case TYPE_SUBTITLE_TTML:
        return SUB_DATA_TYPE_STRING;

    case TYPE_SUBTITLE_CLOSED_CAPTION:
        return SUB_DATA_TYPE_CC_JSON;

    case 0xAAAA:
         return SUB_DATA_TYPE_QTONE;

    }
    return SUB_DATA_TYPE_BITMAP;
}

static int __mapApiType2SubtitleType(int type) {
    SUBTITLE_LOGI("call>> %s [stype:%d]", __func__, type);
    switch (type) {
    case TYPE_SUBTITLE_DVB:
        return DTV_SUB_DVB;
    case TYPE_SUBTITLE_DVB_TELETEXT:
        return DTV_SUB_DVB_TELETEXT;
    case TYPE_SUBTITLE_SCTE27:
        return DTV_SUB_SCTE27;
    case TYPE_SUBTITLE_CLOSED_CAPTION:
        return DTV_SUB_CC;
    case TYPE_SUBTITLE_ARIB_B24:
        return DTV_SUB_ARIB24;
    case TYPE_SUBTITLE_TTML:
        return DTV_SUB_DVB_TTML;
    case TYPE_SUBTITLE_SMPTE_TTML:
        return DTV_SUB_SMPTE_TTML;
    }
    //default:
    return type;
}


class MyAdaptorListener : public SubtitleListener {
public:
    MyAdaptorListener() {
        mDataCb = nullptr;
        mChannelUpdateCb = nullptr;
        mSubtitleAvailCb = nullptr;
        mAfdEventCb = nullptr;
        mDimensionCb  = nullptr;
        mLanguageCb  = nullptr;
        mSubtitleInfoCb  = nullptr;
    }

    virtual ~MyAdaptorListener() {
        mDataCb = nullptr;
        mChannelUpdateCb = nullptr;
        mSubtitleAvailCb = nullptr;
        mAfdEventCb = nullptr;
        mDimensionCb  = nullptr;
        mLanguageCb  = nullptr;
        mSubtitleInfoCb  = nullptr;
    }

    virtual void onSubtitleEvent(const char *data, int size, int parserType,
                int x, int y, int width, int height,
                int videoWidth, int videoHeight, int showing, int objectSegmentId) {
        if (mDataCb != nullptr) {
            mDataCb(data, size,
                __mapServerType2ApiType((AmlSubtitletype)parserType),
                 x, y, width, height, videoWidth, videoHeight, showing);
        }
    }

    virtual void onSubtitleDataEvent(int event, int id) {
        if (mChannelUpdateCb != nullptr) mChannelUpdateCb(event, id);
    }

    virtual void onSubtitleAvail(int avail) {
        if (mSubtitleAvailCb != nullptr) mSubtitleAvailCb(avail);
    }

    virtual void onSubtitleAfdEvent(int event, int playerid) {
        if (mAfdEventCb != nullptr) mAfdEventCb(event);
    }

    virtual void onSubtitleDimension(int width, int height) {
        if (mDimensionCb != nullptr) mDimensionCb(width, height);
    }

    virtual void onSubtitleLanguage(std::string lang) {
        if (mLanguageCb != nullptr) mLanguageCb(lang);
    }

    virtual void onSubtitleInfo(int what, int extra) {
        if (mSubtitleInfoCb != nullptr) mSubtitleInfoCb(what, extra);
    }

    // middleware api do not need this
    virtual void onMixVideoEvent(int val) {}
    virtual void onSubtitleUIEvent(int uiCmd, const std::vector<int> &params) {}


    // sometime, server may crash, we need clean up in server side.
    virtual void onServerDied() {}

    void setupDataCb(AmlSubtitleDataCb cb) { mDataCb = cb; }
    void setupChannelUpdateCb(AmlChannelUpdateCb cb) { mChannelUpdateCb = cb; }
    void setupSubtitleAvailCb(AmlSubtitleAvailCb cb) { mSubtitleAvailCb = cb; }
    void setupAfdEventCb(AmlAfdEventCb cb) { mAfdEventCb = cb; }
    void setupDimensionCb(AmlSubtitleDimensionCb cb) { mDimensionCb = cb; }
    void setupLanguageCb(AmlSubtitleLanguageCb cb) { mLanguageCb = cb; }
    void setupSubtitleInfoCb(AmlSubtitleInfoCb cb) { mSubtitleInfoCb = cb; }


private:
    AmlSubtitleDataCb mDataCb;
    AmlChannelUpdateCb mChannelUpdateCb;
    AmlSubtitleAvailCb mSubtitleAvailCb;
    AmlAfdEventCb mAfdEventCb;
    AmlSubtitleDimensionCb mDimensionCb;
    AmlSubtitleLanguageCb mLanguageCb;
    AmlSubtitleInfoCb mSubtitleInfoCb;
};

class SubtitleContext : public android::RefBase {
public:
    SubtitleContext() {
        SUBTITLE_LOGI("call>> %s [%p]", __func__, this);
        mClient = nullptr;
    }

    ~SubtitleContext() {
        SUBTITLE_LOGI("call>> %s [%p]", __func__, this);
    }

    SubtitleClientLinux *mClient;
    sp<MyAdaptorListener> mAdaptorListener;
};


std::vector<sp<SubtitleContext>> gAvailContextMap;
std::mutex gContextMapLock;

static bool __pushContext(sp<SubtitleContext> ctx) {
    std::unique_lock<std::mutex> autolock(gContextMapLock);
    auto it = gAvailContextMap.begin();
    it = gAvailContextMap.insert (it , ctx);
    SUBTITLE_LOGI("%s>> ctx:%p MapSize=%d", __func__, ctx.get(), gAvailContextMap.size());
    return *it != nullptr;
}

static bool __checkInContextMap(SubtitleContext *ctx) {
    std::unique_lock<std::mutex> autolock(gContextMapLock);
    for (auto it=gAvailContextMap.begin(); it<gAvailContextMap.end(); it++) {
        SUBTITLE_LOGI("%s>> search ctx:%p current:%p MapSize=%d",
                __func__, ctx, (*it).get(), gAvailContextMap.size());

        if (ctx == (*it).get()) {
            return true;
        }
    }
    SUBTITLE_LOGE("Error Invalid context handle!");
    return false;
}

static bool __eraseFromContext(SubtitleContext *ctx) {
    std::unique_lock<std::mutex> autolock(gContextMapLock);
    for (auto it=gAvailContextMap.begin(); it<gAvailContextMap.end(); it++) {

        SUBTITLE_LOGI("%s>> search ctx:%p current:%p MapSize=%d",
                __func__, ctx, (*it).get(), gAvailContextMap.size());

        if (ctx == (*it).get()) {
            gAvailContextMap.erase(it);
            return true;
        }
    }
    SUBTITLE_LOGE("Error Invalid context handle!");
    return false;
}


AmlSubtitleHnd amlsub_Create() {
    SUBTITLE_LOGI("call>> %s", __func__);
    sp<SubtitleContext> ctx = new SubtitleContext();
    SUBTITLE_LOGI("amlsub_Create, mClient= %p", ctx->mClient);
    sp<MyAdaptorListener> listener = new MyAdaptorListener();
    ctx->mClient = new SubtitleClientLinux(false, listener, OpenType::TYPE_MIDDLEWARE);
    ctx->mAdaptorListener = listener;

    __pushContext(ctx);
    return ctx.get();
}

AmlSubtitleStatus amlsub_Destroy(AmlSubtitleHnd handle) {
    SubtitleContext *ctx = (SubtitleContext *) handle;

    SUBTITLE_LOGI("call>> %s handle[%p]", __func__, handle);

    if (ctx == nullptr) {
        return SUB_STAT_INV;
    }

    if (!__checkInContextMap(ctx)) {
        SUBTITLE_LOGE("Error! Bad handle! mis-using API?");
        return SUB_STAT_INV;
    }

    //ctx->mClient = nullptr;
    ctx->mAdaptorListener = nullptr;

    __eraseFromContext(ctx);
    if (ctx != nullptr && ctx->mClient != nullptr) {/*fix crash on amlsub_Destroy*/
        delete ctx->mClient;
        ctx->mClient = nullptr;
    }

    return SUB_STAT_OK;
}


AmlSubtitleStatus amlsub_GetSessionId(AmlSubtitleHnd handle, int *sId) {
    SubtitleContext *ctx = (SubtitleContext *) handle;
    SUBTITLE_LOGI("call>> %s handle[%p]", __func__, handle);
    if (ctx == nullptr) {
        return SUB_STAT_INV;
    }

    if (!__checkInContextMap(ctx)) {
        SUBTITLE_LOGE("Error! Bad handle! mis-using API?");
        return SUB_STAT_INV;
    }

    auto client = ctx->mClient;
    if (client == nullptr) {
        return SUB_STAT_INV;
    } else {
        *sId = client->getSessionId();
    }

    return SUB_STAT_OK;
}


////////////////////////////////////////////////////////////
///////////////////// generic control //////////////////////
////////////////////////////////////////////////////////////
/**
 * open, start to play subtitle.
 * Param:
 *   handle: current subtitle handle.
 *   path: the subtitle external file path.
 */
AmlSubtitleStatus amlsub_Open(AmlSubtitleHnd handle, AmlSubtitleParam *param) {
    SUBTITLE_LOGI("call>> %s handle[%p]", __func__, handle);
    SubtitleContext *ctx = (SubtitleContext *) handle;
    if (ctx == nullptr || ctx->mClient == nullptr) {
        SUBTITLE_LOGE("Error! invalid handle, uninitialized?");
        return SUB_STAT_INV;
    }
    if (!__checkInContextMap(ctx)) {
        SUBTITLE_LOGE("Error! Bad handle! mis-using API?");
        return SUB_STAT_INV;
    }
    if (param == nullptr) {
        SUBTITLE_LOGE("Error! invalid input param, param=%p", param);
        return SUB_STAT_INV;
    }
    ctx->mClient->userDataOpen();
    // Here, we support all the parametes from client to server
    // server will filter the parameter, select to use according from the type.
    ctx->mClient->setSubType(__mapApiType2SubtitleType(param->subtitleType));
    ctx->mClient->setSubPid(param->pid);
    ctx->mClient->setChannelId(param->channelId);
    ctx->mClient->setClosedCaptionVfmt(param->videoFormat);
    //ctx->mClient->setCompositionPageId(param->ancillaryPageId);
    ctx->mClient->applyRenderType();
    // TODO: CTC always use amstream. later we may all change to hwdemux
    // From middleware, the extSubPath should always null.
    SUBTITLE_LOGI("call>> %s demuxId[%d] ioSource[%d]", __func__, param->dmxId, param->ioSource);
    int ioType = -1;
    if (param->dmxId >= 0) {
        ioType = (param->dmxId << 16 | param->ioSource);
    } else {
        ioType = param->ioSource;
    }
    bool r = ctx->mClient->open(param->fd, ioType);
    ctx->mClient->setPageId(param->compositionPageId);
    ctx->mClient->setAncPageId(param->ancillaryPageId);//setAncPageId
    return r ? SUB_STAT_OK : SUB_STAT_FAIL;
}

AmlSubtitleStatus amlsub_Close(AmlSubtitleHnd handle) {
    SUBTITLE_LOGI("call>> %s handle[%p]", __func__, handle);
    SubtitleContext *ctx = (SubtitleContext *)handle;
    if (ctx == nullptr || ctx->mClient == nullptr) {
        return SUB_STAT_INV;
    }
    if (!__checkInContextMap(ctx)) {
        SUBTITLE_LOGE("Error! Bad handle! mis-using API?");
        return SUB_STAT_INV;
    }
    ctx->mClient->userDataClose();
    return ctx->mClient->close() ? SUB_STAT_OK : SUB_STAT_FAIL;
}

/**
 * reset current play, most used for seek.
 */
AmlSubtitleStatus amlsub_Reset(AmlSubtitleHnd handle) {
    SUBTITLE_LOGI("call>> %s handle[%p]", __func__, handle);
    SubtitleContext *ctx = (SubtitleContext *)handle;
    if (ctx == nullptr || ctx->mClient == nullptr) {
        return SUB_STAT_INV;
    }
    if (!__checkInContextMap(ctx)) {
        SUBTITLE_LOGE("Error! Bad handle! mis-using API?");
        return SUB_STAT_INV;
    }
    return ctx->mClient->resetForSeek() ? SUB_STAT_OK : SUB_STAT_FAIL;
}

////////////////////////////////////////////////////////////
//////////////// DTV operation/param related ///////////////
////////////////////////////////////////////////////////////

AmlSubtitleStatus amlsub_SetParameter(AmlSubtitleHnd handle, AmlSubtitleParamCmd cmd, void *value, int paramSize) {
    SUBTITLE_LOGI("call>> %s handle[%p]", __func__, handle);
    SubtitleContext *ctx = (SubtitleContext *)handle;
    if (ctx == nullptr) {
        return SUB_STAT_INV;
    }
    if (!__checkInContextMap(ctx)) {
        SUBTITLE_LOGE("Error! Bad handle! mis-using API?");
        return SUB_STAT_INV;
    }
    return SUB_STAT_OK;
}

int amlsub_GetParameter(AmlSubtitleHnd handle, AmlSubtitleParamCmd cmd, void *value) {
    SUBTITLE_LOGI("call>> %s handle[%p]", __func__, handle);
    return 0;
}

AmlSubtitleStatus amlsub_SetPip(AmlSubtitleHnd handle, AmlSubtitlePipMode mode, int value) {
    SUBTITLE_LOGI("call>> %s handle[%p]", __func__, handle);
    if (value == INVALID_PIP_ID) return SUB_STAT_INV;
    SubtitleContext *ctx = (SubtitleContext *)handle;
    if (ctx == nullptr) {
        return SUB_STAT_INV;
    }
    if (!__checkInContextMap(ctx)) {
        SUBTITLE_LOGE("Error! Bad handle! mis-using API?");
        return SUB_STAT_INV;
    }

    ctx->mClient->setPipId(mode, value);
    return SUB_STAT_OK;
}

AmlSubtitleStatus amlsub_TeletextControl(AmlSubtitleHnd handle, AmlTeletextCtrlParam *param) {
    SUBTITLE_LOGI("call>> %s handle[%p]", __func__, handle);
    SubtitleContext *ctx = (SubtitleContext *)handle;
    if (ctx == nullptr || ctx->mClient == nullptr || param == nullptr) {
        return SUB_STAT_INV;
    }
    if (!__checkInContextMap(ctx)) {
        SUBTITLE_LOGE("Error! Bad handle! mis-using API?");
        return SUB_STAT_INV;
    }

    ctx->mClient->ttControl(param->event, param->magazine, param->page, param->regionid, param->subpagedir);
    return SUB_STAT_OK;
}

AmlSubtitleStatus amlsub_SelectCcChannel(AmlSubtitleHnd handle, int ch) {
    SUBTITLE_LOGI("call>> %s handle[%p] ch:%d", __func__, handle, ch);
    SubtitleContext *ctx = (SubtitleContext *)handle;
    if (ctx == nullptr || ctx->mClient == nullptr || ch < 0) {
        return SUB_STAT_INV;
    }
    if (!__checkInContextMap(ctx)) {
        SUBTITLE_LOGE("Error! Bad handle! mis-using API?");
        return SUB_STAT_INV;
    }

    //ctx->mClient->selectCcChannel(ch);setChannelId
    ctx->mClient->setChannelId(ch);
    return SUB_STAT_OK;
}


////////////////////////////////////////////////////////////
////// Regist callbacks for subtitle Event and data ////////
////// Will be directed display when listener is   ////////
////// null or not called this.                    ////////
////////////////////////////////////////////////////////////
AmlSubtitleStatus amlsub_RegistOnDataCB(AmlSubtitleHnd handle, AmlSubtitleDataCb listener) {
    SUBTITLE_LOGI("call>> %s handle[%p]", __func__, handle);
    SubtitleContext *ctx = (SubtitleContext *) handle;
    if (ctx == nullptr) {
        return SUB_STAT_INV;
    }
    if (!__checkInContextMap(ctx)) {
        SUBTITLE_LOGE("Error! Bad handle! mis-using API?");
        return SUB_STAT_INV;
    }

    ctx->mAdaptorListener->setupDataCb(listener);
    ctx->mClient->setRenderType(listener != nullptr ? CALLBACK_RENDER : DIRECT_RENDER);
    return SUB_STAT_OK;
}

AmlSubtitleStatus amlsub_RegistOnChannelUpdateCb(AmlSubtitleHnd handle, AmlChannelUpdateCb listener) {
    SUBTITLE_LOGI("call>> %s handle[%p]", __func__, handle);
    SubtitleContext *ctx = (SubtitleContext *) handle;
    if (ctx == nullptr) {
        return SUB_STAT_INV;
    }
    if (!__checkInContextMap(ctx)) {
        SUBTITLE_LOGE("Error! Bad handle! mis-using API?");
        return SUB_STAT_INV;
    }
    ctx->mAdaptorListener->setupChannelUpdateCb(listener);
    return SUB_STAT_OK;
}

AmlSubtitleStatus amlsub_RegistOnSubtitleAvailCb(AmlSubtitleHnd handle, AmlSubtitleAvailCb listener) {
    SUBTITLE_LOGI("call>> %s handle[%p]", __func__, handle);
    SubtitleContext *ctx = (SubtitleContext *) handle;
    if (ctx == nullptr) {
        return SUB_STAT_INV;
    }
    if (!__checkInContextMap(ctx)) {
        SUBTITLE_LOGE("Error! Bad handle! mis-using API?");
        return SUB_STAT_INV;
    }
    ctx->mAdaptorListener->setupSubtitleAvailCb(listener);
    return SUB_STAT_OK;
}

AmlSubtitleStatus amlsub_RegistAfdEventCB(AmlSubtitleHnd handle, AmlAfdEventCb listener) {
    SUBTITLE_LOGI("call>> %s handle[%p]", __func__, handle);
    SubtitleContext *ctx = (SubtitleContext *) handle;
    if (ctx == nullptr) {
        return SUB_STAT_INV;
    }
    if (!__checkInContextMap(ctx)) {
        SUBTITLE_LOGE("Error! Bad handle! mis-using API?");
        return SUB_STAT_INV;
    }
    ctx->mAdaptorListener->setupAfdEventCb(listener);
    return SUB_STAT_OK;
}

AmlSubtitleStatus amlsub_RegistGetDimensionCb(AmlSubtitleHnd handle, AmlSubtitleDimensionCb listener) {
    SUBTITLE_LOGI("call>> %s handle[%p]", __func__, handle);
    SubtitleContext *ctx = (SubtitleContext *) handle;
    if (ctx == nullptr) {
        return SUB_STAT_INV;
    }
    if (!__checkInContextMap(ctx)) {
        SUBTITLE_LOGE("Error! Bad handle! mis-using API?");
        return SUB_STAT_INV;
    }
    ctx->mAdaptorListener->setupDimensionCb(listener);
    return SUB_STAT_OK;
}

AmlSubtitleStatus amlsub_RegistOnSubtitleLanguageCb(AmlSubtitleHnd handle, AmlSubtitleLanguageCb listener) {
    SUBTITLE_LOGI("call>> %s handle[%p]", __func__, handle);
    SubtitleContext *ctx = (SubtitleContext *) handle;
    if (ctx == nullptr) {
        return SUB_STAT_INV;
    }
    if (!__checkInContextMap(ctx)) {
        SUBTITLE_LOGE("Error! Bad handle! mis-using API?");
        return SUB_STAT_INV;
    }
    ctx->mAdaptorListener->setupLanguageCb(listener);
    return SUB_STAT_OK;
}

AmlSubtitleStatus amlsub_RegistOnSubtitleInfoCB(AmlSubtitleHnd handle, AmlSubtitleInfoCb listener) {
    SUBTITLE_LOGI("call>> %s handle[%p]", __func__, handle);
    SubtitleContext *ctx = (SubtitleContext *) handle;
    if (ctx == nullptr) {
        return SUB_STAT_INV;
    }
    if (!__checkInContextMap(ctx)) {
        SUBTITLE_LOGE("Error! Bad handle! mis-using API?");
        return SUB_STAT_INV;
    }

    ctx->mAdaptorListener->setupSubtitleInfoCb(listener);

    return SUB_STAT_OK;
}


AmlSubtitleStatus amlsub_UiShow(AmlSubtitleHnd handle) {
    SUBTITLE_LOGI("call>> %s handle[%p]", __func__, handle);
    SubtitleContext *ctx = (SubtitleContext *) handle;
    if (ctx == nullptr || ctx->mClient == nullptr) {
        return SUB_STAT_INV;
    }
    if (!__checkInContextMap(ctx)) {
        SUBTITLE_LOGE("Error! Bad handle! mis-using API?");
        return SUB_STAT_INV;
    }

    bool r = ctx->mClient->uiShow();
    return r ? SUB_STAT_OK : SUB_STAT_FAIL;
}

AmlSubtitleStatus amlsub_UiHide(AmlSubtitleHnd handle) {
    SUBTITLE_LOGI("call>> %s handle[%p]", __func__, handle);
    SubtitleContext *ctx = (SubtitleContext *) handle;
    if (ctx == nullptr || ctx->mClient == nullptr) {
        return SUB_STAT_INV;
    }
    if (!__checkInContextMap(ctx)) {
        SUBTITLE_LOGE("Error! Bad handle! mis-using API?");
        return SUB_STAT_INV;
    }

    bool r = ctx->mClient->uiHide();
    return r ? SUB_STAT_OK : SUB_STAT_FAIL;
}

/* Only available for text subtitle */
AmlSubtitleStatus amlsub_UiSetTextColor(AmlSubtitleHnd handle, int color) {
    SUBTITLE_LOGI("call>> %s handle[%p]", __func__, handle);
    SubtitleContext *ctx = (SubtitleContext *) handle;
    if (ctx == nullptr || ctx->mClient == nullptr) {
        return SUB_STAT_INV;
    }
    if (!__checkInContextMap(ctx)) {
        SUBTITLE_LOGE("Error! Bad handle! mis-using API?");
        return SUB_STAT_INV;
    }

    bool r = ctx->mClient->uiSetTextColor(color);
    return r ? SUB_STAT_OK : SUB_STAT_FAIL;
}

AmlSubtitleStatus amlsub_UiSetTextSize(AmlSubtitleHnd handle, int sp) {
    SUBTITLE_LOGI("call>> %s handle[%p]", __func__, handle);
    SubtitleContext *ctx = (SubtitleContext *) handle;
    if (ctx == nullptr || ctx->mClient == nullptr) {
        return SUB_STAT_INV;
    }
    if (!__checkInContextMap(ctx)) {
        SUBTITLE_LOGE("Error! Bad handle! mis-using API?");
        return SUB_STAT_INV;
    }

    bool r = ctx->mClient->uiSetTextSize(sp);
    return r ? SUB_STAT_OK : SUB_STAT_FAIL;
}

AmlSubtitleStatus amlsub_UiSetGravity(AmlSubtitleHnd handle, int gravity) {
    SUBTITLE_LOGI("call>> %s handle[%p]", __func__, handle);
    SubtitleContext *ctx = (SubtitleContext *) handle;
    if (ctx == nullptr || ctx->mClient == nullptr) {
        return SUB_STAT_INV;
    }
    if (!__checkInContextMap(ctx)) {
        SUBTITLE_LOGE("Error! Bad handle! mis-using API?");
        return SUB_STAT_INV;
    }
    bool r = ctx->mClient->uiSetGravity(gravity);
    return r ? SUB_STAT_OK : SUB_STAT_FAIL;
}

AmlSubtitleStatus amlsub_UiSetPosHeight(AmlSubtitleHnd handle, int yOffset) {
    SUBTITLE_LOGI("call>> %s handle[%p]", __func__, handle);
    SubtitleContext *ctx = (SubtitleContext *) handle;
    if (ctx == nullptr || ctx->mClient == nullptr) {
        return SUB_STAT_INV;
    }
    if (!__checkInContextMap(ctx)) {
        SUBTITLE_LOGE("Error! Bad handle! mis-using API?");
        return SUB_STAT_INV;
    }

    bool r = ctx->mClient->uiSetYOffset(yOffset);
    return r ? SUB_STAT_OK : SUB_STAT_FAIL;
}

AmlSubtitleStatus amlsub_UiSetImgRatio(AmlSubtitleHnd handle, float ratioW, float ratioH, int maxW, int maxH) {
    SUBTITLE_LOGI("call>> %s handle[%p]", __func__, handle);
    SubtitleContext *ctx = (SubtitleContext *) handle;
    if (ctx == nullptr || ctx->mClient == nullptr) {
        return SUB_STAT_INV;
    }
    if (!__checkInContextMap(ctx)) {
        SUBTITLE_LOGE("Error! Bad handle! mis-using API?");
        return SUB_STAT_INV;
    }

    bool r = ctx->mClient->uiSetImageRatio(ratioW, ratioH, maxW, maxH);
    return r ? SUB_STAT_OK : SUB_STAT_FAIL;
}

AmlSubtitleStatus amlsub_UiSetSurfaceViewRect(AmlSubtitleHnd handle, int x, int y, int width, int height) {
    SUBTITLE_LOGI("call>> %s handle[%p]", __func__, handle);
    SubtitleContext *ctx = (SubtitleContext *) handle;
    if (ctx == nullptr || ctx->mClient == nullptr) {
        return SUB_STAT_INV;
    }
    if (!__checkInContextMap(ctx)) {
        SUBTITLE_LOGE("Error! Bad handle! mis-using API?");
        return SUB_STAT_INV;
    }

    bool r = ctx->mClient->uiSetSurfaceViewRect(x, y, width, height);
    return r ? SUB_STAT_OK : SUB_STAT_FAIL;
}

AmlSubtitleStatus amlsub_UpdateVideoPos(AmlSubtitleHnd handle, int64_t pos) {

    SUBTITLE_LOGI("call>> %s handle[%p]", __func__, handle);
    SubtitleContext *ctx = (SubtitleContext *)handle;
    if (ctx == nullptr || ctx->mClient == nullptr) {
        return SUB_STAT_INV;
    }
    if (!__checkInContextMap(ctx)) {
        SUBTITLE_LOGE("Error! Bad handle! mis-using API?");
        return SUB_STAT_INV;
    }

    bool r  = ctx->mClient->updateVideoPos(pos);
    return r ? SUB_STAT_OK : SUB_STAT_FAIL;
}
