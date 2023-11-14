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

#define LOG_TAG "Scte27Parser"
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <list>
#include <thread>
#include <algorithm>
#include <functional>
#include <mutex>

//#include "trace_support.h"
#include "SubtitleLog.h"
#include <utils/CallStack.h>

#include "sub_types2.h"
#include "Scte27Parser.h"
#include "ParserFactory.h"


#define  DATA_SIZE 10*1024

// we want access Parser in little-dvb callback.
// must use a global lock
static std::mutex gLock;

Scte27Parser *Scte27Parser::sInstance = nullptr;
Scte27Parser *Scte27Parser::getCurrentInstance() {
    return Scte27Parser::sInstance;
}

void Scte27Parser::notifySubtitleDimension(int width, int height) {
    if (mNotifier != nullptr) {
        SUBTITLE_LOGI("notifySubtitleDimension: %d %d", width, height);
        mNotifier->onSubtitleDimension(width, height);
    }
}
static void save2BitmapFile(const char *filename, uint32_t *bitmap, int w, int h) {
    SUBTITLE_LOGI("save2BitmapFile:%s\n",filename);
    FILE *f;
    char fname[40];

    snprintf(fname, sizeof(fname), "%s.bmp", filename);
    f = fopen(fname, "w");
    if (!f) {
        perror(fname);
        return;
    }

    fprintf(f, "P6\n %d %d\n%d\n", w, h, 255);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int v = bitmap[y * w + x];
            putc((v >> 16) & 0xff, f);
            putc((v >> 8) & 0xff, f);
            putc((v >> 0) & 0xff, f);
        }
    }
    fclose(f);
}


static inline void clearBitmap(TVSubtitleData *sub) {
    uint8_t *ptr = sub->buffer;
    int y = sub->bmp_h;

    while (y--) {
        memset(ptr, 0, sub->bmp_pitch);
        ptr += sub->bmp_pitch;
    }
}

static uint8_t *lockBitmap(int *w, int *h, int *bpp) {
    uint8_t *pbuf = NULL;
    *w = 1920;
    *h = 1080;
    *bpp = 4;
    pbuf = (unsigned char *)malloc((*w)*(*h)*(*bpp));    //SCTE27_SUB_SIZE need *4

    if (!pbuf) {
        SUBTITLE_LOGI("malloc pbuf failed!\n");
        return NULL;
    }

    return pbuf;
}

static void unlockBitmap(TVSubtitleData *ctx) {
    if (ctx != nullptr && ctx->buffer != nullptr) {
        free(ctx->buffer);
        ctx->buffer = NULL;
    }
}



static uint8_t *getBitmap(int *w, int *h, int *bpp) {
    uint8_t *buf;

    buf = lockBitmap(w, h, bpp);
    SUBTITLE_LOGI("bitmap buffer [%p]", buf);
    return buf;
}


static void initBitmap(TVSubtitleData *ctx, int bitmap_w, int bitmap_h) {
    if (ctx == nullptr) {
        return;
    }

    if (!ctx->buffer) {
        ctx->buffer = getBitmap(&ctx->bmp_w, &ctx->bmp_h, &ctx->bmp_pitch);
    }
    ctx->sub_w = bitmap_w;
    ctx->sub_h = bitmap_h;

    SUBTITLE_LOGI("init_bitmap w:%d h:%d p:%d", ctx->bmp_w, ctx->bmp_h, ctx->bmp_pitch);


}

static void updateSizeCallback(AM_SCTE27_Handle_t handle, int width, int height) {
    SUBTITLE_LOGI("scte27_update_size width:%d,height:%d", width, height);
    TVSubtitleData *sub = (TVSubtitleData *)AM_SCTE27_GetUserData(handle);
    if (!sub) {
        SUBTITLE_LOGE("Error!Scte27 get userdata null! ");
        return;
    }
    pthread_mutex_lock(&sub->lock);
    sub->sub_w = width;
    sub->sub_h = height;
    //setScte27WidthHeight(width, height);
    Scte27Parser *parser = Scte27Parser::getCurrentInstance();
    if (parser == nullptr) {
        SUBTITLE_LOGE("Report subtitle size to a deleted scte parser!");
        pthread_mutex_unlock(&sub->lock);
        return;
    }
    parser->notifySubtitleDimension(sub->sub_w, sub->sub_h);
    pthread_mutex_unlock(&sub->lock);
}

static void drawBeginCallback(AM_SCTE27_Handle_t handle) {
    SUBTITLE_LOGI("[scte27_draw_begin_cb]");
    (void)handle;
}

static void drawEndCallback(AM_SCTE27_Handle_t handle) {
    SUBTITLE_LOGI("[scte27_draw_end_cb]");
    TVSubtitleData *sub = (TVSubtitleData *)AM_SCTE27_GetUserData(handle);

    std::unique_lock<std::mutex> autolock(gLock);
    Scte27Parser *parser = Scte27Parser::getCurrentInstance();
    if (parser == nullptr) {
        SUBTITLE_LOGE("Report subtitle string to a deleted scte parser!");
        return;
    }

    TVSubtitleData *ctx = parser->getContexts();
    std::shared_ptr<AML_SPUVAR> spu(new AML_SPUVAR());
    spu->buffer_size = sub->sub_w*sub->sub_h*4;//SCTE27_SUB_SIZE;
    spu->spu_data = (unsigned char *)malloc(sub->sub_w*sub->sub_h*4);
    if (!spu->spu_data) {
        SUBTITLE_LOGI("av_malloc SCTE27_SUB_SIZE failed!\n");
        return;
    }
    memset(spu->spu_data, 0, sub->sub_w*sub->sub_h*4);
    memcpy(spu->spu_data,ctx->buffer, sub->sub_w*sub->sub_h*4);
    spu->spu_width = sub->sub_w;
    spu->spu_height = sub->sub_h;
    SUBTITLE_LOGI("[scte27_draw_end_cb]data->buffer:%p", ctx->buffer);
    spu->spu_origin_display_w = ctx->sub_w;
    spu->spu_origin_display_h = ctx->sub_h;

    // CC and scte use little dvb, render&presentation already handled.
    spu->isImmediatePresent = true;
    parser->addDecodedItem(std::shared_ptr<AML_SPUVAR>(spu));

    {
        bool enableDump = false;
#ifdef ANDROID
        char value[PROPERTY_VALUE_MAX] = {0};
        memset(value, 0, PROPERTY_VALUE_MAX);
        property_get("vendor.subtitle.dump", value, "false");
        enableDump = strcmp(value, "true") == 0;
#endif
        if (enableDump) {
            static int sIndex = 0;
            char filename[32];
            snprintf(filename, sizeof(filename), "./data/subtitleDump/27_%d", sIndex++);
            save2BitmapFile(filename, (uint32_t *)ctx->buffer, 1920, 1080);
            SUBTITLE_LOGI("[scte27_draw_end_cb]dump success!");
        }
    }
    memset(ctx->buffer, 0, sub->sub_w*sub->sub_h*4);
    SUBTITLE_LOGI("[scte27_draw_end_cb]");

}

static void reportCallback(AM_SCTE27_Handle_t handle, int error) {
    (void)handle;
    std::unique_lock<std::mutex> autolock(gLock);
    Scte27Parser *parser = Scte27Parser::getCurrentInstance();
    if (parser != nullptr) {
        parser->notifyCallerAvail(__notifierErrorRemap(error));
    }
}

static void reportAvailableCallback(AM_SCTE27_Handle_t handle) {
    (void)handle;
    std::unique_lock<std::mutex> autolock(gLock);
    Scte27Parser *parser = Scte27Parser::getCurrentInstance();
    if (parser != nullptr) {
        parser->notifyCallerAvail(1);
    }
}

static void langCallback(AM_SCTE27_Handle_t handle, char *buffer, int size) {
    (void)handle;
    std::unique_lock<std::mutex> autolock(gLock);
    Scte27Parser *parser = Scte27Parser::getCurrentInstance();
    SUBTITLE_LOGI("%s, lang:%s, size:%d", __FUNCTION__, buffer, size);
    if (buffer == NULL || size == 0) {
        return;
    }

    if (parser != nullptr) {
        parser->notifyCallerLanguage(buffer);
    }
}

Scte27Parser::Scte27Parser(std::shared_ptr<DataSource> source) {
    SUBTITLE_LOGI("creat Scte27 Parser");
    mPid = -1;
    mDataSource = source;
    mParseType = TYPE_SUBTITLE_SCTE27;
    std::unique_lock<std::mutex> autolock(gLock);
    sInstance = this;
    mScteContext = new TVSubtitleData();
}

Scte27Parser::~Scte27Parser() {
    SUBTITLE_LOGI("%s", __func__);
    {
        std::unique_lock<std::mutex> autolock(gLock);
        sInstance = nullptr;
    }

    stopScte27();
    stopParser();

    delete mScteContext;
}

void Scte27Parser::notifyCallerAvail(int avil) {
    if (mNotifier != nullptr) {
        mNotifier->onSubtitleAvailable(avil);
    }
}

void Scte27Parser::notifyCallerLanguage(char *lang) {
    if (mNotifier != nullptr) {
        mNotifier->onSubtitleLanguage(lang);
    }
}


bool Scte27Parser::updateParameter(int type, void *data) {
    (void) type;
    Scte27Param *scte27_p = (Scte27Param *) data;
    mPid = scte27_p->SCTE27_PID;
    SUBTITLE_LOGI("updateParameter mPid:%d", mPid);
    return true;
}

void Scte27Parser::setPipId (int mode, int id) {
    SUBTITLE_LOGI(" setPipId mode:%d, id %d", mode, id);
    if (PIP_PLAYER_ID == mode) {
        mPlayerId = id;
    } else if (PIP_MEDIASYNC_ID == mode) {
        SUBTITLE_LOGI(" setPipId  mMediaSyncId = %d, id=%d,mState=%d",mMediaSyncId,id,mState);
       if ((mMediaSyncId != id) && ( mState == SUB_PLAYING)) {
           mMediaSyncId = id;
           stopScte27();
           startScte27(0, 0);
           return;
        }
       mMediaSyncId = id;
    }
}

void Scte27Parser::checkDebug() {
    //AM_DebugSetLogLevel(property_get_int32("tv.dvb.loglevel", 1));
}

int Scte27Parser::startScte27(int dmxId, int pid) {
    SUBTITLE_LOGI("[sub_start_scte27]");
    int ret;
    SUBTITLE_LOGE("[sub_start_scte27]pid:%d, dmx_id:%d", pid, dmxId);
    AM_SCTE27_Para_t sctep;
   // struct dmx_sct_filter_params param;

    checkDebug();

    initBitmap(mScteContext, VIDEO_W, VIDEO_H);

    mScteContext->dmx_id = dmxId;
    memset(&sctep, 0, sizeof(sctep));
    sctep.width  = VIDEO_W;
    sctep.height = VIDEO_H;
    sctep.draw_begin        = drawBeginCallback;
    sctep.draw_end          = drawEndCallback;
    sctep.lang_cb           = langCallback;
    sctep.report            = reportCallback;
    sctep.report_available  = reportAvailableCallback;
    sctep.bitmap        = &mScteContext->buffer;
    sctep.pitch         = BITMAP_PITCH;/*data->bmp_pitch;*/
    sctep.update_size   = updateSizeCallback;
    sctep.user_data     = mScteContext;
    sctep.lang_cb       = langCallback;
    sctep.media_sync = mMediaSyncId;
    ret = AM_SCTE27_Create(&mScteContext->scte27_handle, &sctep);
    if (ret != AM_SUCCESS) {
        goto error;
    }

    ret = AM_SCTE27_Start(mScteContext->scte27_handle);
    if (ret != AM_SUCCESS) {
        goto error;
    }

   mData = std::shared_ptr<uint8_t>(new uint8_t[DATA_SIZE], std::default_delete<uint8_t[]>());

    return 0;
error:
    SUBTITLE_LOGE("scte start failed");
    if (mScteContext->scte27_handle) {
        AM_SCTE27_Destroy(mScteContext->scte27_handle);
        mScteContext->scte27_handle = NULL;
    }
    return 0;
}


int Scte27Parser::stopScte27() {
    AM_SCTE27_Destroy(mScteContext->scte27_handle);
    mScteContext->dmx_id = -1;
    mScteContext->filter_handle = -1;

    pthread_mutex_lock(&mScteContext->lock);
    //data->buffer = lockBitmap(env, data->obj_bitmap);
    clearBitmap(mScteContext);
    unlockBitmap(mScteContext);

    pthread_mutex_unlock(&mScteContext->lock);

    mScteContext->scte27_handle = NULL;
    mScteContext->pes_handle = NULL;
    return 0;
}

static int getScte27Spu( ) {
    int ret = 0;
    Scte27Parser *parser = Scte27Parser::getCurrentInstance();
   uint8_t* tmpbuf = parser->mData.get();
   memset(tmpbuf, 0, 10*1024);
   int size = parser->mDataSource->read(tmpbuf, 0xffff);
   if (size <= 0)
       return -1;
   ret = AM_SCTE27_Decode(parser->mScteContext->scte27_handle, tmpbuf, size);
   return ret;
}

int Scte27Parser::getSpu() {
    if (mState == SUB_INIT) {
        mState = SUB_PLAYING;
    } else if (mState == SUB_STOP) {
        SUBTITLE_LOGI(" mState == SUB_STOP \n\n");
        return 0;
    }
    return getScte27Spu();
}
int Scte27Parser::parse() {
    startScte27(0, 0);
    while (!mThreadExitRequested) {
        if (getSpu() < 0) {
            // advance input, if parse failed, wait and retry.
            //std::this_thread::sleep_for(std::chrono::milliseconds(1));
            usleep(100);
        }
    }
    return 0;

}

void Scte27Parser::dump(int fd, const char *prefix) {
    dprintf(fd, "%s SCTE 27 Parser\n", prefix);
    dumpCommon(fd, prefix);
    dprintf(fd, "\n");
    dprintf(fd, "%s   Program Id: %d\n", prefix, mPid);
    dprintf(fd, "\n");
    if (mScteContext != nullptr) {
        mScteContext->dump(fd, prefix);
    }

}

