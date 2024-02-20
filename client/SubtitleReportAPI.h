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

#ifndef __SUBTITLE_SOCKET_CLIENT_API_H__
#define __SUBTITLE_SOCKET_CLIENT_API_H__

#include "SubtitleAPICommon.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *SubSourceHandle;

//typedef enum {
//    SUB_STAT_INV = -1,
//    SUB_STAT_FAIL = 0,
//    SUB_STAT_OK,
//}SubSourceStatus;

enum CodecID {
    /* subtitle codecs */
    CODEC_ID_DVD_SUBTITLE = 0x17000,
    CODEC_ID_DVB_SUBTITLE,
    CODEC_ID_TEXT,  ///< raw UTF-8 text
    CODEC_ID_XSUB,
    CODEC_ID_SSA,
    CODEC_ID_MOV_TEXT,
    CODEC_ID_HDMV_PGS_SUBTITLE,
    CODEC_ID_DVB_TELETEXT,
    CODEC_ID_SRT,

    CODEC_ID_MICRODVD = 0x17800,
    CODEC_ID_EIA_608,
    CODEC_ID_JACOSUB,
    CODEC_ID_SAMI,
    CODEC_ID_REALTEXT,
    CODEC_ID_STL,
    CODEC_ID_SUBVIEWER1,
    CODEC_ID_SUBVIEWER,
    CODEC_ID_SUBRIP,
    CODEC_ID_WEBVTT,
    CODEC_ID_ML2,
    CODEC_ID_VPLAYER,
    CODEC_ID_PJS,
    CODEC_ID_ASS,
    CODEC_ID_HDMV_TEXT_SUBTITLE,
    CODEC_ID_TTML_SUBTITLE,
    CODEC_ID_SMPTE_TTML_SUBTITLE,
};

SubSourceHandle SubSource_Create(int sId);
SubSourceStatus SubSource_Destroy(SubSourceHandle handle);

SubSourceStatus SubSource_Reset(SubSourceHandle handle);
SubSourceStatus SubSource_Stop(SubSourceHandle handle);

SubSourceStatus SubSource_ReportRenderTime(SubSourceHandle handle, int64_t timeUs);
SubSourceStatus SubSource_ReportStartPts(SubSourceHandle handle, int64_t type);

SubSourceStatus SubSource_ReportTotalTracks(SubSourceHandle handle, int trackNum);
SubSourceStatus SubSource_ReportType(SubSourceHandle handle, int type);

SubSourceStatus SubSource_ReportSubTypeString(SubSourceHandle handle, const char *type);
SubSourceStatus SubSource_ReportLauguageString(SubSourceHandle handle, const char *lang);


//SubSourceStatus SubSource_SendData(SubSourceHandle handle, const char *data, int size);
SubSourceStatus SubSource_SendData(SubSourceHandle handle, const char *mbuf, int length, int64_t pts,
        enum CodecID sub_type) ;

SubSourceStatus SubSource_GetVersion(SubSourceHandle handle, int *version) {
    *version = 1;
    return SUB_STAT_OK;
};

#ifdef __cplusplus
}
#endif


#endif
