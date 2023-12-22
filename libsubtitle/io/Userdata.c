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

#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif

#define  LOG_TAG "Userdata"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <math.h>
#include <signal.h>
#include <sys/utsname.h>
#include <stdbool.h>

#include "SubtitleLog.h"

#include "UserdataDriver.h"
#include "MediaSyncInterface.h"
#include "cutils/properties.h"


#define USERDATA_POLL_TIMEOUT 100
#define MAX_CC_NUM          64
#define MAX_CC_DATA_LEN  (1024*5 + 4)

#define IS_H264(p)  ((p[0] == 0xb5 && p[3] == 0x47 && p[4] == 0x41 && p[5] == 0x39 && p[6] == 0x34))
#define IS_H265(p)  ((p[0] == 0xb5 && p[3] == 0x47 && p[4] == 0x41 && p[5] == 0x39 && p[6] == 0x34))
#define IS_DIRECTV(p) ((p[0] == 0xb5 && p[1] == 0x00 && p[2] == 0x2f))
#define IS_AVS(p)    ((p[0] == 0x47) && (p[1] == 0x41) && (p[2] == 0x39) && (p[3] == 0x34))
#define IS_ATSC(p)  ((p[0] == 0x47) && (p[1] == 0x41) && (p[2] == 0x39) && (p[3] == 0x34))
#define IS_SCTE(p)  ((p[0]==0x3) && ((p[1]&0x7f) == 1))

#define IS_AFD(p)   ((p[0] == 0x44) && (p[1] == 0x54) && (p[2] == 0x47) && (p[3] == 0x31))
#define IS_H264_AFD(p)  ((p[0] == 0xb5) && (p[3] == 0x44) && (p[4] == 0x54) && (p[5] == 0x47) && (p[6] == 0x31))

#define AMSTREAM_IOC_MAGIC  'S'
#define AMSTREAM_IOC_UD_LENGTH _IOR(AMSTREAM_IOC_MAGIC, 0x54, unsigned long)
#define AMSTREAM_IOC_UD_POC _IOR(AMSTREAM_IOC_MAGIC, 0x55, int)
#define AMSTREAM_IOC_UD_FLUSH_USERDATA _IOR(AMSTREAM_IOC_MAGIC, 0x56, int)
#define AMSTREAM_IOC_UD_BUF_READ _IOR(AMSTREAM_IOC_MAGIC, 0x57, int)
#define AMSTREAM_IOC_UD_AVAILABLE_VDEC      _IOR(AMSTREAM_IOC_MAGIC, 0x5c, unsigned int)

/****************************************************************************
 * Type definitions
 ***************************************************************************/

typedef enum {
    INVALID_TYPE = 0,
    CC_TYPE_FOR_JUDGE = 10,
    MPEG_CC_TYPE,
    H264_CC_TYPE,
    DIRECTV_CC_TYPE,
    AVS_CC_TYPE,
    SCTE_CC_TYPE,
    AFD_TYPE_FOR_JUDGE = 100,
    MPEG_AFD_TYPE,
    H264_AFD_TYPE,
    USERDATA_TYPE_MAX,
} userdata_type;

#define IS_AFD_TYPE(p) (p > AFD_TYPE_FOR_JUDGE && p < USERDATA_TYPE_MAX)
#define IS_CC_TYPE(p) (p > CC_TYPE_FOR_JUDGE && p < AFD_TYPE_FOR_JUDGE)
typedef enum {
    /* 0 forbidden */
    I_TYPE = 1,
    P_TYPE = 2,
    B_TYPE = 3,
    D_TYPE = 4,
    /* 5 ... 7 reserved */
} picture_coding_type;

struct userdata_meta_info_t {
    uint32_t poc_number;
    /************ flags bit definition ***********
    bit 0:      //used for mpeg2
        1, group start
        0, not group start
    bit 1-2:    //used for mpeg2
        0, extension_and_user_data( 0 )
        1, extension_and_user_data( 1 )
        2, extension_and_user_data( 2 )
    bit 3-6:    //video format
        0,  VFORMAT_MPEG12
        1,  VFORMAT_MPEG4
        2,  VFORMAT_H264
        3,  VFORMAT_MJPEG
        4,  VFORMAT_REAL
        5,  VFORMAT_JPEG
        6,  VFORMAT_VC1
        7,  VFORMAT_AVS
        8,  VFORMAT_SW
        9,  VFORMAT_H264MVC
        10, VFORMAT_H264_4K2K
        11, VFORMAT_HEVC
        12, VFORMAT_H264_ENC
        13, VFORMAT_JPEG_ENC
        14, VFORMAT_VP9
    bit 7-9:    //frame type
        0, Unknown Frame Type
        1, I Frame
        2, B Frame
        3, P Frame
        4, D_Type_MPEG2
    bit 10:  //top_field_first_flag valid
        0: top_field_first_flag is not valid
        1: top_field_first_flag is valid
    bit 11: //top_field_first bit val
    bit 12-13: //picture_struct, used for H264
        0: Invalid
        1: TOP_FIELD_PICTURE
        2: BOT_FIELD_PICTURE
        3: FRAME_PICTURE
    **********************************************/
    uint32_t flags;
    uint32_t vpts;          /*video frame pts*/
    /******************************************
    0: pts is invalid, please use duration to calculate
    1: pts is valid
    ******************************************/
    uint32_t vpts_valid;
    /*duration for frame*/
    uint32_t duration;
    /* how many records left in queue waiting to be read*/
    uint32_t records_in_que;
    unsigned long long priv_data;
    uint32_t padding_data[4];
};


struct userdata_param_t {
    uint32_t version;
    uint32_t instance_id; /*input, 0~9*/
    uint32_t buf_len; /*input*/
    uint32_t data_size; /*output*/
    void* pbuf_addr; /*input*/
    struct userdata_meta_info_t meta_info; /*output*/
};

typedef struct AM_CCData_s AM_CCData;
struct AM_CCData_s {
    AM_CCData *next;
    uint8_t   *buf;
    int64_t pts;
    int64_t duration;
    int pts_valid;
    int     size;
    int     cap;
    int     poc;
};

typedef struct {
    uint32_t picture_structure:16;
    uint32_t temporal_reference:10;
    uint32_t picture_coding_type:3;
    uint32_t reserved:3;
    uint32_t index:16;
    uint32_t offset:16;
    uint8_t  atsc_flag[4];
    uint8_t  cc_data_start[4];
} aml_ud_header_t;

typedef struct {
    pthread_t      th;
    int          fd;
    int vfmt;
    int playerId;
    int mediasync_id;
    bool      running;
    AM_CCData     *cc_list;
    AM_CCData     *free_list;
    int          cc_num;
    userdata_type   format;
    int          curr_poc;
    int64_t curr_pts;
    int64_t curr_duration;
    int scte_enable;
    int mode;
} AM_UDDrvData;

#define MOD_ON(__mod, __mask) (__mod & __mask)
#define MOD_ON_CC(__mod) MOD_ON(__mod, USERDATA_MODE_CC)
#define MOD_ON_AFD(__mod) MOD_ON(__mod, USERDATA_MODE_AFD)


/****************************************************************************
 * Static data definitions
 ***************************************************************************/

static int aml_open(UserdataDeviceType *dev, const UserdataOpenParaType *para);
static int aml_close(UserdataDeviceType *dev);
static int aml_set_mode(UserdataDeviceType *dev, int mode);
static int aml_get_mode(UserdataDeviceType *dev, int *mode);
static int aml_set_param(UserdataDeviceType *dev, int para);

const UserdataDriverType aml_ud_drv = {
.open  = aml_open,
.close = aml_close,
.set_mode = aml_set_mode,
.get_mode = aml_get_mode,
.set_param = aml_set_param,
};

int64_t am_get_video_pts(void* media_sync) {
    #define VIDEO_PTS_PATH "/sys/class/tsync/pts_video"
    int64_t value = 0;
    if (media_sync != NULL) {
        MediaSync_getTrackMediaTime(media_sync, &value);
        value = 0xFFFFFFFF & ((9*value)/100);//now userdata pass only 32bit.
        return value;
    } else {
        char buffer[16] = {0};
        AmlogicFileRead(VIDEO_PTS_PATH,buffer,16);
        return strtoul(buffer, NULL, 16);
    }
}

static void dump_cc_data(char *who, int poc, uint8_t *buff, int size) {
    int i;
    char buf[4096];
    if (size > 1024)
        size = 1024;
    for (i=0; i<size; i++) {
        sprintf(buf+i*3, "%02x ", buff[i]);
    }
    SUBTITLE_LOGI("CC DUMP:who:%s poc: %d :%s", who, poc, buf);
}

static void aml_free_cc_data (AM_CCData *cc) {
    if (cc->buf) free(cc->buf);
    free(cc);
}

static void aml_swap_data(uint8_t *user_data, int ud_size) {
    int swap_blocks, i, j, k, m;
    unsigned char c_temp;
    /* swap byte order */
    swap_blocks = ud_size >> 3;
    for (i = 0; i < swap_blocks; i ++) {
        j = i << 3;
        k = j + 7;
        for (m=0; m<4; m++) {
            c_temp = user_data[j];
            user_data[j++] = user_data[k];
            user_data[k--] = c_temp;
        }
    }
}

static uint8_t scte20_char_map[256];
static uint8_t scte20_get_char (uint8_t c) {
    if (scte20_char_map[1] == 0) {
        int i;
        for (i = 0; i < 256; i ++) {
            uint8_t v1, v2;
            int     b;
            v1 = i;
            v2 = 0;
            for (b = 0; b < 8; b ++) {
                if (v1 & (1 << b))
                    v2 |= (1 << (7 - b));
            }
            scte20_char_map[i] = v2;
        }
    }
    return scte20_char_map[c];
}

static userdata_type aml_check_userdata_format (uint8_t *buf, int vfmt, int len) {
    if (len < 8)
        return INVALID_TYPE;
    //vfmt:
    //0 for MPEG
    //2 for H264
    //7 for AVS

    if (vfmt == 2) {
        if (IS_H264(buf) || IS_H265(buf)) {
            SUBTITLE_LOGI("CC format is h264_or_h265_cc_type");
            return H264_CC_TYPE;
        } else if (IS_DIRECTV(buf)) {
            SUBTITLE_LOGI("CC format is directv_cc_type");
            return DIRECTV_CC_TYPE;
        } else if (IS_H264_AFD(buf)) {
            return H264_AFD_TYPE;
        }
    } else if (vfmt == 7) {
        if (IS_AVS(buf)) {
            SUBTITLE_LOGI("CC format is avs_cc_type");
            return AVS_CC_TYPE;
        }
    } else if (vfmt == 0) {
        if (len >= (int)sizeof(aml_ud_header_t)) {
            aml_ud_header_t *hdr = (aml_ud_header_t*)buf;
            if (IS_ATSC(hdr->atsc_flag)) {
                SUBTITLE_LOGI("CC format is mpeg_cc_type");
                return MPEG_CC_TYPE;
            } else if (IS_SCTE(hdr->atsc_flag)) {
                SUBTITLE_LOGI("CC format is scte_cc_type");
                return SCTE_CC_TYPE;
            } else if (IS_AFD(hdr->atsc_flag)) {
                return MPEG_AFD_TYPE;
            }
        }
    } // else {
    //    SUBTITLE_LOGE("vfmt not handled");
    // }
    return INVALID_TYPE;
}

static void aml_write_userdata(UserdataDeviceType *dev, uint8_t *buffer, int buffer_len, int64_t pts, int pts_valid, int64_t duration) {
    int64_t *pts_in_buffer;
    uint8_t userdata_with_pts[MAX_CC_DATA_LEN];
    AM_UDDrvData *ud = dev->drv_data;
    if (pts_valid == 0) pts = ud->curr_pts + ud->curr_duration;
    pts_in_buffer = userdata_with_pts;
    *pts_in_buffer = pts;
    memcpy(userdata_with_pts+4, buffer, buffer_len);
    dev->write_package(dev, userdata_with_pts, buffer_len + 4);
    ud->curr_pts = pts;
    ud->curr_duration = duration;
}

static void aml_flush_cc_data(UserdataDeviceType *dev) {
    AM_UDDrvData *ud = dev->drv_data;
    AM_CCData *cc, *ncc;
    char  buf[256];
    char *pp = buf;
    int   left = sizeof(buf), i, pr;

    for (cc = ud->cc_list; cc; cc = ncc) {
        ncc = cc->next;

        for (i = 0; i < cc->size; i ++) {
            pr = snprintf(pp, left, "%02x ", cc->buf[i]);
            if (pr < left) {
                pp   += pr;
                left -= pr;
            }
        }

        SUBTITLE_LOGI("cc_write_package decode:%s", buf);

        aml_write_userdata(dev, cc->buf, cc->size, cc->pts, cc->pts_valid, cc->duration);

        cc->next = ud->free_list;
        ud->free_list = cc;
    }

    ud->cc_list  = NULL;
    ud->cc_num   = 0;
    ud->curr_poc = -1;
}

static void aml_add_cc_data(UserdataDeviceType *dev, int poc, int type, uint8_t *p, int len, int64_t pts, int pts_valid, int64_t duration) {
    AM_UDDrvData *ud = dev->drv_data;
    AM_CCData **pcc, *cc;
    char  buf[256];

    //for coverity, this code has no effect.0830version index 176
    //char *pp = buf;
    //int   left = sizeof(buf), i, pr;

    if (ud->cc_num >= MAX_CC_NUM) {
#if 0
        aml_flush_cc_data(dev);
#else
        cc = ud->cc_list;
        //dump_cc_data("add cc ",cc->poc,cc->buf, cc->size);
        aml_write_userdata(dev, cc->buf, cc->size, cc->pts, cc->pts_valid, cc->duration);

        ud->cc_list = cc->next;
        cc->next = ud->free_list;
        ud->free_list = cc;
        ud->cc_num --;
#endif
    }

    //for coverity, this code has no effect.
    /*for (i = 0; i < len; i ++) {
        pr = snprintf(pp, left, "%02x ", p[i]);
        if (pr < left) {
            pp   += pr;
            left -= pr;
        }
    }*/

    SUBTITLE_LOGI("CC poc:%d ptype:%d data:%s", poc, type, buf);

    pcc = &ud->cc_list;
    if (*pcc && poc < ((*pcc)->poc - 30))
        aml_flush_cc_data(dev);

    while ((cc = *pcc)) {
        /*if (cc->poc == poc) {
            aml_flush_cc_data(dev);
            pcc = &ud->cc_list;
            break;
        }*/
        if (cc->pts == pts)
        {
            if (cc->poc > poc)
                break;
        }
        else if (cc->pts > pts) {
            break;
        }

        pcc = &cc->next;
    }

    if (ud->free_list) {
        cc = ud->free_list;
        ud->free_list = cc->next;
    } else {
        cc = malloc(sizeof(AM_CCData));
        if (!cc) {
            SUBTITLE_LOGI("Error! cc data malloc failed!");
            return;
        }
        cc->buf  = NULL;
        cc->size = 0;
        cc->cap  = 0;
        cc->poc  = 0;
    }

    if (cc->cap < len) {
        cc->buf = realloc(cc->buf, len);
        if (!cc->buf) {
            SUBTITLE_LOGI("Error! cc buf realloc failed!");
            return;
        }
        cc->cap = len;
    }

    memcpy(cc->buf, p, len);

    cc->size = len;
    cc->poc  = poc;
    cc->pts = pts;
    cc->pts_valid = pts_valid;
    cc->duration = duration;
    cc->next = *pcc;
    *pcc = cc;

    ud->cc_num ++;
}

static void aml_mpeg_userdata_package(UserdataDeviceType *dev, int poc, int type, uint8_t *p, int len, int64_t pts, int pts_valid, int64_t duration) {
    AM_UDDrvData *ud = dev->drv_data;
#if 0
    int i;
    char display_buffer[10240];
    for (i=0;i<len; i++)
        sprintf(&display_buffer[i*3], " %02x", p[i]);
    SUBTITLE_LOGI("mpeg_process len:%d data:%s", len, display_buffer);
#endif

    if (len < 5) return;
    if (p[4] != 3) return;

    if (type == I_TYPE) aml_flush_cc_data(dev);
    if (poc == ud->curr_poc + 1 || poc == ud->curr_poc) {
        AM_CCData *cc, **pcc;
        aml_write_userdata(dev, p, len, pts, pts_valid, duration);
        ud->curr_poc = poc;
        pcc = &ud->cc_list;
        while ((cc = *pcc)) {
            if (ud->curr_poc + 1 != cc->poc) break;
            aml_write_userdata(dev, cc->buf, cc->size, cc->pts, cc->pts_valid, cc->duration);
            *pcc = cc->next;
            ud->curr_poc ++;
            cc->next = ud->free_list;
            ud->free_list = cc;
        }
        return;
    }
    aml_add_cc_data(dev, poc, type, p, len, pts, pts_valid, duration);
}

static int aml_process_scte_userdata(UserdataDeviceType *dev, uint8_t *data, int len, struct userdata_meta_info_t* meta_info) {
    int cc_count = 0, cnt, i;
    int field_num;
    uint8_t* cc_section;
    uint8_t cc_data[64] = {0};
    uint8_t* scte_head;
    uint8_t* scte_head_search_position;
    int head_posi = 0;
    int prio, field, line, cc1, cc2, mark, size, ptype, ref;
    int write_position, bits = 0, array_position = 0;
    uint8_t *p;
    int left = len;
    int left_bits;
    uint32_t v;
    int flags;
    int64_t pts;
    int top_bit_value, top_bit_valid;
    AM_UDDrvData *ud = dev->drv_data;

#if 0
    char display_buffer[10240];
#endif

    flags = meta_info->flags;
    pts = 0;//meta_info->vpts;

    if (ud->scte_enable != 1) return len;
    v = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
    top_bit_valid = flags & (1<<10);
    top_bit_value = flags & (1<<11);
    if (top_bit_valid == 0) top_bit_value = 1;
    ref = (v >> 16) & 0x3ff;
    ptype = (v >> 26) & 7;

#if 0
    for (i=0; i<len; i++)
        sprintf(display_buffer+3*i, " %02x", data[i]);
    SUBTITLE_LOGI("scte buffer type %d ref %d top_bit %d %s", ptype, ref, top_bit_value, display_buffer);
#endif

    if (ptype == I_TYPE)  aml_flush_cc_data(dev);

    scte_head = data;
    while (head_posi < len) {
        scte_head_search_position = &scte_head[head_posi];
        if (IS_SCTE(scte_head_search_position)) break;
        head_posi += 8;
    }

    if ((len - head_posi) < 8)
        return len;

    p = &data[head_posi + 2];
    cc_data[0] = 0x47;
    cc_data[1] = 0x41;
    cc_data[2] = 0x39;
    cc_data[3] = 0x34;
    cc_data[4] = 0x3;

    left_bits = (len - head_posi) << 3;

#define NST_BITS(v, b, l) (((v) >> (b)) & (0xff >> (8 - (l))))
#define GET(n, b)\
    do \
    {\
        int off, bs;\
        if (bits + b > left_bits) goto error;\
        off = bits >> 3;\
        bs  = bits & 7;\
        if (8 - bs >= b) {\
          n = NST_BITS(p[off], 8 - bs - b, b);\
        } else {\
          int n1, n2, b1 = 8 - bs, b2 = b- b1;\
          n1 = NST_BITS(p[off], 0, b1);\
          n2 = NST_BITS(p[off + 1], 8 - b + b1, b - b1);\
          n = (n1 << b2) | n2;\
        }\
        bits += b;\
    } while(0)

    GET(cnt, 5);
    array_position = 7;
    for (i = 0; i < cnt; i ++) {
        GET(prio, 2);
        GET(field, 2);
        GET(line, 5);
        GET(cc1, 8);
        GET(cc2, 8);
        GET(mark, 1);
        if (field == 3)
            field = 1;
        SUBTITLE_LOGI("loop %d field %d line %d cc1 %x cc2 %x",
            i, field, line, cc1, cc2);
        if (field == 1)
            line = (top_bit_value)?line+10:line+273;
        else if (field == 2)
            line = (top_bit_value)?line+273:line+10;
        else
            continue;

        if (line == 21) {
            cc_data[array_position] = 4;
            cc_data[array_position + 1] = scte20_get_char(cc1);
            cc_data[array_position + 2] = scte20_get_char(cc2);
            array_position += 3;
            cc_count++;
        } else if (line == 284) {
            cc_data[array_position] = 4|1;
            cc_data[array_position + 1] = scte20_get_char(cc1);
            cc_data[array_position + 2] = scte20_get_char(cc2);
            array_position += 3;
            cc_count++;
        }
        /*else
            continue;*/ //for coverity useless_continue
    }
    cc_data[5] = 0x40 |cc_count;
    size = 7 + cc_count*3;

#if 0
    for (i=0; i<size; i++)
            sprintf(display_buffer+3*i, " %02x", cc_data[i]);
        //SUBTITLE_LOGI("scte_write_buffer len: %d data: %s", size, display_buffer);
#endif
    if (cc_count > 0)
        aml_add_cc_data(dev, ref, ptype, cc_data, size, pts, meta_info->vpts_valid,
            meta_info->duration);
error:
    return len;
}

static int aml_process_mpeg_userdata(UserdataDeviceType *dev, uint8_t *data, int len, struct userdata_meta_info_t* meta_info) {
    AM_UDDrvData *ud = dev->drv_data;
    uint8_t *pd = data;
    int left = len;
    int r = 0;
    int i;
    int package_count = 0;
    int userdata_length;
    int flag;
    int64_t pts;

    flag = meta_info->flags;
    pts = meta_info->vpts;

    while (left >= (int)sizeof(aml_ud_header_t)) {
        aml_ud_header_t *hdr = (aml_ud_header_t*)pd;
        int ref, ptype;

        if (MOD_ON_CC(ud->mode) && IS_ATSC(hdr->atsc_flag) ) {
            aml_ud_header_t *nhdr;
            uint8_t *pp, t;
            uint32_t v;
            int pl;

            //pp = (uint8_t*)&hdr->atsc_flag;
            pp = pd + 8;//for coverity, in case p[4] atsc flag overrun
            pl = 8;

            v = (pd[4] << 24) | (pd[5] << 16) | (pd[6] << 8) | pd[7];
            ref   = (v >> 16) & 0x3ff;
            ptype = (v >> 26) & 7;

            /* We read one packet in one time, so treat entire buffer */
            if (!(flag & (1<<2)))
                return len;

            userdata_length = len-r-8;
            aml_mpeg_userdata_package(dev, ref, ptype, pp, userdata_length, pts, meta_info->vpts_valid, meta_info->duration);
            r = len;

            break;
        } else if (MOD_ON_AFD(ud->mode) && IS_AFD(hdr->atsc_flag)) {
            uint8_t *pafd_hdr = (uint8_t*)hdr->cc_data_start;
            UserdataAFDType afd = *((UserdataAFDType *)(pafd_hdr));
            //afd.reserved = afd.pts = 0;
            AmlogicEventSignal(dev->dev_no, USERDATA_EVENT_AFD, (void*)&afd);
            pd += 8;
            left -= 8;
            r += 8;
            break;
        } else {
            pd   += 8;
            left -= 8;
            r += 8;
        }
    }
    return r;
}

static void aml_h264_userdata_package(UserdataDeviceType *dev, int poc, int type, uint8_t *p, int len, int64_t pts, int pts_valid, int64_t duration, void* media_sync) {
    AM_UDDrvData *ud = dev->drv_data;
    AM_CCData *cc;
    //if (poc == 0) aml_flush_cc_data(dev);
    aml_add_cc_data(dev, poc, I_TYPE, p, len, pts, pts_valid, duration);
    cc = ud->cc_list;
    while (cc) {
        if (am_get_video_pts(media_sync) > cc->pts)
        {
            aml_write_userdata(dev, cc->buf, cc->size, cc->pts, cc->pts_valid, cc->duration);
            ud->curr_poc = cc->poc;
            //If node is chain head, reset chain head
            if (cc == ud->cc_list) {
                ud->cc_list = cc->next;
            } else {
                //Delete node from chain
                AM_CCData *cc_tmp = ud->cc_list;
                while (cc_tmp) {
                    if (cc_tmp->next == cc) {
                        cc_tmp->next = cc->next;
                        break;
                    } else {
                        cc_tmp = cc_tmp->next;
                    }
                }
            }
            cc->next = ud->free_list;
            ud->free_list = cc;
            ud->cc_num --;
            cc = ud->cc_list;
        }
        else
        {
            cc = cc->next;
        }
    }
}

static int aml_process_h264_userdata(UserdataDeviceType *dev, uint8_t *data, int len, struct userdata_meta_info_t* meta_info, void* media_sync) {
    AM_UDDrvData *ud = dev->drv_data;
    int fd = ud->fd;
    uint8_t *pd = data;
    int left = len;
    int r = 0;
    int poc;
    int64_t pts;

    poc = meta_info->poc_number;
    pts = meta_info->vpts;

    while (left >= 8) {
        if (MOD_ON_CC(ud->mode)
            && ((IS_H264(pd) || IS_H265(pd) || IS_DIRECTV(pd) || IS_AVS(pd)))) {
            int hdr = (ud->format == H264_CC_TYPE) ? 3 : 0;
            int pl;

            pd += hdr;

            pl = 8 + (pd[5] & 0x1f) * 3;

            if (pl + hdr > left) {
                break;
            }

            //SUBTITLE_LOGI("CC poc_number:%x hdr:%d pl:%d", poc, hdr, pl);
            if (poc == 0) {
                aml_flush_cc_data(dev);
            }

            //aml_add_cc_data(dev, poc, I_TYPE, pd, pl, pts, meta_info->vpts_valid,
            //  meta_info->duration);

            //aml_add_cc_data(dev, poc, I_TYPE, userdata_with_pts, pl+4);
            aml_h264_userdata_package(dev, poc, I_TYPE, pd, pl, pts, meta_info->vpts_valid,
                meta_info->duration, media_sync);

            pd   += pl;
            left -= pl + hdr;
            r   += pl + hdr;
        } else if (MOD_ON_AFD(ud->mode) && IS_H264_AFD(pd)) {
            UserdataAFDType afd = *((UserdataAFDType*)(pd + 7));
            //afd.reserved = afd.pts = 0;
            AmlogicEventSignal(dev->dev_no, USERDATA_EVENT_AFD, (void*)&afd);
            pd   += 8;
            left -= 8;
            r    += 8;
            break;
        } else {
            pd   += 8;
            left -= 8;
            r   += 8;
        }
    }

    return r;
}

static int get_kernel_version(void) {
    int version, subversion, patchlevel;
    struct utsname utsn;
    /* Return 0 on failure, and attempt to probe with empty kversion */
    if (uname(&utsn)) return 0;
    if (sscanf(utsn.release, "%d.%d.%d", &version, &subversion, &patchlevel) != 3) return 0;
    SUBTITLE_LOGI("get_kernel_version version= %d,subversion=%d",version,subversion);
    return (version << 16) + (subversion << 8) + patchlevel;
}

static void* aml_userdata_thread (void *arg) {
    UserdataDeviceType *dev = (UserdataDeviceType*)arg;
    AM_UDDrvData *ud = (AM_UDDrvData*)dev->drv_data;
    int fd = ud->fd;
    int r, ret, i;
    struct pollfd pfd;
    uint8_t data[MAX_CC_DATA_LEN];
    uint8_t *pd = data;
#if 0
    char display_buffer[10*1024];
#endif
    int left = 0;
    int flush = 1;
    int vdec_ids;
    int read_vdec_id;
    struct userdata_param_t user_para_info;

    pfd.events = POLLIN|POLLERR;
    pfd.fd = fd;
    void* media_sync = NULL;
    if (ud->mediasync_id >= 0) {
        media_sync = MediaSync_create();
        if (NULL != media_sync) {
            MediaSync_bindInstance(media_sync, ud->mediasync_id, MEDIA_SUBTITLE);
        }
    }
    int kernel_version = get_kernel_version();
    kernel_version = kernel_version>>16;
    while (ud->running) {
        //If scte and mpeg both exist, we need to ignore scte cc data,
        //so we need to check cc type every time.
        left = 0;
        ret = poll(&pfd, 1, USERDATA_POLL_TIMEOUT);
        // SUBTITLE_LOGI("userdata after poll ret %d", ret);
        if (!ud->running) break;

        if (media_sync == NULL && ud->mediasync_id >= 0) {
            media_sync = MediaSync_create();
            if (NULL != media_sync) {
                MediaSync_bindInstance(media_sync, ud->mediasync_id, MEDIA_SUBTITLE);
            }
        }

        if (ret != 1)
            continue;
        if (!(pfd.revents & POLLIN))
            continue;
        //For multi-instances support
        vdec_ids = 0;

        if (-1 == ioctl(fd, AMSTREAM_IOC_UD_AVAILABLE_VDEC, &vdec_ids)) {
            SUBTITLE_LOGI("get available vdec failed");
            continue;
        } // else {
        //    SUBTITLE_LOGI("get available vdec OK: 0x%x\n", vdec_ids);
        // }

        if (kernel_version >= 5 || property_get_int32("ro.build.version.sdk", 28) >= 29) {
        //SUBTITLE_LOGI("kernel version:%d, android sdk:%d",kernel_version, property_get_int32("ro.build.version.sdk", 28));
            read_vdec_id = vdec_ids;
        } else {
            read_vdec_id = ffs(vdec_ids) - 1;
        }

        // if (read_vdec_id != ud->playerId) {
        //     SUBTITLE_LOGI("the player is not consistent with the video read_vdec_id=%d,ud->playerId=%d",read_vdec_id,ud->playerId);
        //     continue;
        // }

        if (flush) {
            ioctl(fd, AMSTREAM_IOC_UD_FLUSH_USERDATA, &read_vdec_id);
            flush = 0;
            continue;
        }

        do {
            if (!ud->running)
                break;
            if (left < 8) {
            memset(&user_para_info, 0, sizeof(struct userdata_param_t));
            user_para_info.pbuf_addr = (void*)(size_t)data;
            user_para_info.buf_len = sizeof(data);
            user_para_info.instance_id = read_vdec_id;

            if (-1 == ioctl(fd, AMSTREAM_IOC_UD_BUF_READ, &user_para_info))
                SUBTITLE_LOGI("call AMSTREAM_IOC_UD_BUF_READ failed\n");
    //          SUBTITLE_LOGI("vdec_id %d real_id %d ioctl left data: %d",
    //          vdec_ids, read_vdec_id, user_para_info.meta_info.records_in_que);

            r = user_para_info.data_size;
            r = (r > MAX_CC_DATA_LEN) ? MAX_CC_DATA_LEN : r;

            if (r <= 0)
                continue;
            aml_swap_data(data + left, r);
            left += r;
            pd = data;
            }else {
                usleep(20*1000);
            }
#if 0
            static char display_buffer[10*1024];
            for (i=0; i<left; i++)
                sprintf(&display_buffer[i*3], " %02x", data[i]);
            SUBTITLE_LOGI("fmt %d ud_aml_buffer: %s", ud->vfmt, display_buffer);
#endif
            ud->format = INVALID_TYPE;
            while (ud->format == INVALID_TYPE || IS_AFD_TYPE(ud->format)) {
                if (left < 8) break;

                ud->format = aml_check_userdata_format(pd, ud->vfmt, left);
                if (!IS_CC_TYPE(ud->format) && !IS_AFD_TYPE(ud->format)) {
                    pd   += 8;
                    left -= 8;
                } else {
                    break;
                }
            }

            if ((ud->format == MPEG_CC_TYPE) || (ud->format == MPEG_AFD_TYPE)) {
                ud->scte_enable = 0;
                //TODO coverity should be fixed index 176
                r = aml_process_mpeg_userdata(dev, pd, left, &user_para_info.meta_info);
            } else if (ud->format == SCTE_CC_TYPE) {
                if (ud->scte_enable == 1)
                    r = aml_process_scte_userdata(dev, pd, left, &user_para_info.meta_info);
                else
                    r = left;
            } else if (ud->format != INVALID_TYPE) {
                r = aml_process_h264_userdata(dev, pd, left, &user_para_info.meta_info, media_sync);
            } else {
                r = left;
            }

        if ((data != pd + r) && (r < left)) {
            memmove(data, pd + r, left - r);
        }

        left -= r;
        }while(user_para_info.meta_info.records_in_que > 1 || (left >= 8));
    }
    SUBTITLE_LOGI("aml userdata thread exit");
    if (media_sync != NULL) {
        MediaSync_destroy(media_sync);
    }
    return NULL;
}

static int aml_open(UserdataDeviceType *dev, const UserdataOpenParaType *para) {
    AM_UDDrvData *ud = malloc(sizeof(AM_UDDrvData));
    int r;
    if (!ud) {
        SUBTITLE_LOGI("not enough memory");
        return USERDATA_ERROR_NO_MEM;
    }
    ud->fd = open("/dev/amstream_userdata", O_RDONLY);
    if (ud->fd == -1) {
        SUBTITLE_LOGI("cannot open userdata device");
        free(ud);
        return USERDATA_ERROR_CANNOT_OPEN_DEV;
    }
    ud->vfmt = para->vfmt;
    ud->playerId = para->playerid;
    ud->mediasync_id = para->mediasyncid;
    ud->format  = INVALID_TYPE;
    ud->cc_list   = NULL;
    ud->free_list = NULL;
    ud->running   = true;
    ud->cc_num  = 0;
    ud->curr_poc  = -1;
    ud->scte_enable = 1;
    if (!para->cc_default_stop) ud->mode = USERDATA_MODE_CC;
    dev->drv_data = ud;
    r = pthread_create(&ud->th, NULL, aml_userdata_thread, (void*)dev);
    if (r) {
        SUBTITLE_LOGI("create userdata thread failed");
        close(ud->fd);
        free(ud);
        return USERDATA_ERROR_SYS;
    }
    AM_SigHandlerInit();
    return AM_SUCCESS;
}

static int aml_close(UserdataDeviceType *dev) {
    AM_UDDrvData *ud = dev->drv_data;
    AM_CCData *cc, *cc_next;
    ud->running = 0;
    pthread_kill(ud->th, SIGALRM);
    pthread_join(ud->th, NULL);
    close(ud->fd);
    for (cc = ud->cc_list; cc; cc = cc_next) {
        cc_next = cc->next;
        aml_free_cc_data(cc);
    }
    for (cc = ud->free_list; cc; cc = cc_next) {
        cc_next = cc->next;
        aml_free_cc_data(cc);
    }
    free (ud);
    return AM_SUCCESS;
}

static int aml_set_mode(UserdataDeviceType *dev, int mode) {
    AM_UDDrvData *ud = dev->drv_data;
    if (MOD_ON_CC(mode) != MOD_ON_CC(ud->mode)) {
        if (MOD_ON_CC(mode)) {
            ud->mode |= USERDATA_MODE_CC;
        } else {
            ud->mode &= ~USERDATA_MODE_CC;
        }
    }
    if (MOD_ON_AFD(mode) != MOD_ON_AFD(ud->mode)) {
        if (MOD_ON_AFD(mode)) {
            ud->mode |= USERDATA_MODE_AFD;
        } else {
            ud->mode &= ~USERDATA_MODE_AFD;
        }
    }
    return AM_SUCCESS;
}

static int aml_get_mode(UserdataDeviceType *dev, int *mode) {
    AM_UDDrvData *ud = dev->drv_data;
    *mode = ud->mode;
    return AM_SUCCESS;
}

static int aml_set_param(UserdataDeviceType *dev, int para) {
    if (dev == NULL) return USERDATA_ERROR_INVALID_ARG;
    AM_UDDrvData *ud = dev->drv_data;
    ud->mediasync_id = para;
    return AM_SUCCESS;
}
