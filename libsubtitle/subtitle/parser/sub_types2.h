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

#ifndef _SUB_TYPE2_H
#define _SUB_TYPE2_H

//#include <am_sub2.h>
//#include <am_tt2.h>
#include <am_dmx.h>
#include <am_pes.h>
#include <am_misc.h>
#include <am_cc.h>
#include <am_userdata.h>


//#include <am_isdb.h>
#include <am_scte27.h>

#include <stdlib.h>
#include <pthread.h>
//#include "trace_support.h"
#include "SubtitleLog.h"
#include <utils/CallStack.h>

#define CC_JSON_BUFFER_SIZE 8192

#define PIP_PLAYER_ID 1
#define PIP_MEDIASYNC_ID 2


typedef struct {
    pthread_mutex_t  lock;
    //AM_SUB2_Handle_t sub_handle;
    AM_PES_Handle_t  pes_handle;
    //AM_TT2_Handle_t  tt_handle;
    AM_CC_Handle_t   cc_handle;
    AM_SCTE27_Handle_t scte27_handle;
    int              dmx_id;
    int              filter_handle;
    //jobject        obj;
    //SkBitmap       *bitmap;
    //jobject        obj_bitmap;
    int              bmp_w;
    int              bmp_h;
    int              bmp_pitch;
    uint8_t          *buffer;
    int              sub_w;
    int              sub_h;

    void dump(int fd, const char* prefix) {
        dprintf(fd, "%s   SubtitleDataContext:\n", prefix);
        dprintf(fd, "%s     handle[pes:%p cc:%p scte:%p]\n", prefix, pes_handle, cc_handle, scte27_handle);
        dprintf(fd, "%s     dmx_id:%d\n", prefix, dmx_id);
        dprintf(fd, "%s     filter_handle:%d\n", prefix, filter_handle);
        dprintf(fd, "%s     bitmap size:%dx%d\n", prefix, bmp_w, bmp_h);
        dprintf(fd, "%s     bitmap pitch:%d\n", prefix, bmp_pitch);
        dprintf(fd, "%s     subtitle size:%dx%d\n", prefix, sub_w, sub_h);
        dprintf(fd, "%s     buffer:%p\n", prefix, buffer);
    }
} TVSubtitleData;




#endif
