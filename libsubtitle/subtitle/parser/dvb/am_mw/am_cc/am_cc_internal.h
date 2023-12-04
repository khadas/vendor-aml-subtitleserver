/***************************************************************************
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
 *
 * Description:
 */
/**\file am_cc_internal.h
 * \brief CC module internal data
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2011-12-27: create the document
 ***************************************************************************/

#ifndef _AM_CC_INTERNAL_H
#define _AM_CC_INTERNAL_H

#include <pthread.h>
#include <libzvbi.h>
#include <dtvcc.h>
#include <am_cc.h>
#include <am_types.h>

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/


/****************************************************************************
 * Type definitions
 ***************************************************************************/

enum
{
    AM_CC_EVT_SET_USER_OPTIONS,
};

enum
{
    FLASH_NONE,
    FLASH_SHOW,
    FLASH_HIDE
};

enum
{
    UNKNOWN = -1,
    MPEG12,
    MPEG4,
    H264,
    MJPEG,
    REAL,
    JPEG,
    VC1,
    AVS,
    SW,
    H264MVC,
    H264_4K2K,
    HEVC,
    H264_ENC,
    JPEG_ENC,
    VP9,
};


typedef struct AM_CC_Decoder AM_CC_Decoder_t;

typedef struct json_chain AM_CC_JsonChain_t;
#define JSON_STRING_LENGTH 8192
struct json_chain
{
    char* buffer;
    uint32_t pts;
    struct timespec decode_time;
    struct timespec display_time;
    struct json_chain* json_chain_next;
    struct json_chain* json_chain_prior;
    int count;
};

/**\brief ClosedCaption数据*/
struct AM_CC_Decoder
{
    int vfmt;
    int evt;
    int vbi_pgno;
    int flash_stat;
    int timeout;
    AM_Bool_t running;
    AM_Bool_t hide;
    AM_Bool_t render_flag;
    AM_Bool_t need_clear;
    AM_Bool_t auto_detect_play;
    AM_Bool_t auto_set_play_flag;
    AM_Bool_t cea_708_cc_flag;
    int auto_detect_last_708_pgno;
    int auto_detect_last_pgno;
    int auto_detect_pgno_count;
    unsigned int decoder_param;
    char lang[12];
    pthread_t render_thread;
    pthread_t data_thread;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    struct tvcc_decoder decoder;

    AM_CC_JsonChain_t* json_chain_head;
    AM_CC_CreatePara_t cpara;
    AM_CC_StartPara_t spara;

    int curr_data_mask;
    int curr_switch_mask;
    int process_update_flag;
    int media_sync_id;
    int player_id;
    uint32_t decoder_cc_pts;
    uint32_t video_pts;
};


/****************************************************************************
 * Function prototypes
 ***************************************************************************/
int tvcc_to_json (struct tvcc_decoder *td, int pgno, char *buf, size_t len);
int tvcc_empty_json (int pgno, char *buf, size_t len);



#ifdef __cplusplus
}
#endif

#endif

