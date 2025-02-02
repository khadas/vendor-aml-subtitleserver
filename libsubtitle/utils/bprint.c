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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
//#include <avassert.h>
//#include <avstring.h>
#include "bprint.h"


//#include <common.h>
//#include "compat/va_copy.h"
//#include "error.h"
//#include "mem.h"

#define av_bprint_room(buf) ((buf)->size - FFMIN((buf)->len, (buf)->size))
#define av_bprint_is_allocated(buf) ((buf)->str != (buf)->reserved_internal_buffer)
#if 1

#if 1
void *av_realloc(void *ptr, unsigned int size)
{
    return realloc(ptr, size);
}


void *av_malloc(unsigned int size)
{
    void *ptr = NULL;

    ptr = malloc(size);
    return ptr;
}

void av_freep(void **arg)
{
    if (*arg)
      free(*arg);
    *arg = NULL;
}
void av_free(void *arg)
{
    if (arg)
      free(arg);
}
void *av_mallocz(unsigned int size)
{
    void *ptr = malloc(size);
    if (ptr)
        memset(ptr, 0, size);
    return ptr;
}

#endif

int ff_fast_malloc(void *ptr, unsigned int *size, size_t min_size, int zero_realloc)
{
    void *val;

    memcpy(&val, ptr, sizeof(val));
    if (min_size <= *size) {
        //av_assert0(val || !min_size);
        return 0;
    }
    min_size = FFMAX(min_size + min_size / 16 + 32, min_size);
    av_freep(ptr);
    val = zero_realloc ? av_mallocz(min_size) : av_malloc(min_size);
    memcpy(ptr, &val, sizeof(val));
   if (!val) {
       av_free(val);
       min_size = 0;
   }
    *size = min_size;
    return 1;
}


void av_fast_padded_malloc(void *ptr, unsigned int *size, size_t min_size)
{
    uint8_t **p = ptr;
    if (min_size > SIZE_MAX - AV_INPUT_BUFFER_PADDING_SIZE) {
        av_freep((void **)p);
        *size = 0;
        return;
    }
    if (!ff_fast_malloc(p, size, min_size + AV_INPUT_BUFFER_PADDING_SIZE, 1))
        memset(*p + min_size, 0, AV_INPUT_BUFFER_PADDING_SIZE);
}


#endif


static int av_bprint_alloc(AVBPrint *buf, unsigned room)
{
    char *old_str, *new_str;
    unsigned min_size, new_size;

    if (buf->size == buf->size_max)
        return -1;
    if (!av_bprint_is_complete(buf))
        return -1; /* it is already truncated anyway */
    min_size = buf->len + 1 + FFMIN(UINT_MAX - buf->len - 1, room);
    new_size = buf->size > buf->size_max / 2 ? buf->size_max : buf->size * 2;
    if (new_size < min_size)
        new_size = FFMIN(buf->size_max, min_size);
    old_str = av_bprint_is_allocated(buf) ? buf->str : NULL;
    new_str = av_realloc(old_str, new_size);
    if (!new_str)
        return -1;
    if (!old_str)
        memcpy(new_str, buf->str, buf->len + 1);
    buf->str  = new_str;
    buf->size = new_size;
    return 0;
}

static void av_bprint_grow(AVBPrint *buf, unsigned extra_len)
{
    /* arbitrary margin to avoid small overflows */
    extra_len = FFMIN(extra_len, UINT_MAX - 5 - buf->len);
    buf->len += extra_len;
    if (buf->size)
       buf->str[FFMIN(buf->len, buf->size - 1)] = 0;
}

void av_bprint_init(AVBPrint *buf, unsigned size_init, unsigned size_max)
{
    unsigned size_auto = (char *)buf + sizeof(*buf) -
                         buf->reserved_internal_buffer;

    if (size_max == 1)
        size_max = size_auto;
    buf->str      = buf->reserved_internal_buffer;
    buf->len      = 0;
    buf->size     = FFMIN(size_auto, size_max);
    buf->size_max = size_max;
    *buf->str = 0;
    if (size_init > buf->size)
        av_bprint_alloc(buf, size_init - 1);
}

#if 0
void av_bprint_init_for_buffer(AVBPrint *buf, char *buffer, unsigned size)
{
   buf->str      = buffer;
    buf->len      = 0;
    buf->size     = size;
    buf->size_max = size;
    *buf->str = 0;
}
#endif
void av_bprintf(AVBPrint *buf, const char *fmt, ...)
{
    unsigned room;
    char *dst;
    va_list vl;
    int extra_len;

    while (1) {
        room = av_bprint_room(buf);
        dst = room ? buf->str + buf->len : NULL;
        va_start(vl, fmt);
        extra_len = vsnprintf(dst, room, fmt, vl);
        va_end(vl);
        if (extra_len <= 0)
            return;
        if (extra_len < room)
            break;
        if (av_bprint_alloc(buf, extra_len))
            break;
    }
    av_bprint_grow(buf, extra_len);
}

#if 0
void av_vbprintf(AVBPrint *buf, const char *fmt, va_list vl_arg)
{
    unsigned room;
    char *dst;
    int extra_len;
   va_list vl;

    while (1) {
        room = av_bprint_room(buf);
        dst = room ? buf->str + buf->len : NULL;
        va_copy(vl, vl_arg);
        extra_len = vsnprintf(dst, room, fmt, vl);
        va_end(vl);
        if (extra_len <= 0)
            return;
        if (extra_len < room)
            break;
        if (av_bprint_alloc(buf, extra_len))
            break;
   }
    av_bprint_grow(buf, extra_len);
}

void av_bprint_chars(AVBPrint *buf, char c, unsigned n)
{
    unsigned room, real_n;

    while (1) {
        room = av_bprint_room(buf);
        if (n < room)
            break;
        if (av_bprint_alloc(buf, n))
            break;
    }
    if (room) {
        real_n = FFMIN(n, room - 1);
        memset(buf->str + buf->len, c, real_n);
    }
    av_bprint_grow(buf, n);
}

void av_bprint_append_data(AVBPrint *buf, const char *data, unsigned size)
{
    unsigned room, real_n;

    while (1) {
        room = av_bprint_room(buf);
        if (size < room)
            break;
        if (av_bprint_alloc(buf, size))
            break;
    }
    if (room) {
        real_n = FFMIN(size, room - 1);
        memcpy(buf->str + buf->len, data, real_n);
    }
    av_bprint_grow(buf, size);
}

void av_bprint_strftime(AVBPrint *buf, const char *fmt, const struct tm *tm)
{
    unsigned room;
    size_t l;

    if (!*fmt)
        return;
    while (1) {
        room = av_bprint_room(buf);
        if (room && (l = strftime(buf->str + buf->len, room, fmt, tm)))
            break;
        /* strftime does not tell us how much room it would need: let us
           retry with twice as much until the buffer is large enough */
        room = !room ? strlen(fmt) + 1 :
               room <= INT_MAX / 2 ? room * 2 : INT_MAX;
        if (av_bprint_alloc(buf, room)) {
            /* impossible to grow, try to manage something useful anyway */
            room = av_bprint_room(buf);
            if (room < 1024) {
                /* if strftime fails because the buffer has (almost) reached
                   its maximum size, let us try in a local buffer; 1k should
                   be enough to format any real date+time string */
                char buf2[1024];
                if ((l = strftime(buf2, sizeof(buf2), fmt, tm))) {
                    av_bprintf(buf, "%s", buf2);
                    return;
                }
            }
            if (room) {
                /* if anything else failed and the buffer is not already
                   truncated, let us add a stock string and force truncation */
                static const char txt[] = "[truncated strftime output]";
                memset(buf->str + buf->len, '!', room);
                memcpy(buf->str + buf->len, txt, FFMIN(sizeof(txt) - 1, room));
                av_bprint_grow(buf, room); /* force truncation */
            }
            return;
        }
    }
    av_bprint_grow(buf, l);
}

void av_bprint_get_buffer(AVBPrint *buf, unsigned size,
                          unsigned char **mem, unsigned *actual_size)
{
    if (size > av_bprint_room(buf))
        av_bprint_alloc(buf, size);
    *actual_size = av_bprint_room(buf);
    *mem = *actual_size ? buf->str + buf->len : NULL;
}
#endif
void av_bprint_clear(AVBPrint *buf)
{
    if (buf->len) {
        *buf->str = 0;
        buf->len  = 0;
    }
}






int av_bprint_finalize(AVBPrint *buf, char **ret_str)
{
    unsigned real_size = FFMIN(buf->len + 1, buf->size);
    char *str;
    int ret = 0;

    if (ret_str) {
        if (av_bprint_is_allocated(buf)) {
            str = av_realloc(buf->str, real_size);
            if (!str)
                str = buf->str;
            buf->str = NULL;
        } else {
            str = av_malloc(real_size);
            if (str)
                memcpy(str, buf->str, real_size);
            else
                ret = -1;//AVERROR(ENOMEM);
        }
        *ret_str = str;
    } else {
        if (av_bprint_is_allocated(buf))
            av_freep((void **)(&buf->str));
    }
    buf->size = real_size;
    return ret;
}
#if 0

#define WHITESPACES " \n\t\r"

void av_bprint_escape(AVBPrint *dstbuf, const char *src, const char *special_chars,
                      enum AVEscapeMode mode, int flags)
{
   const char *src0 = src;

    if (mode == AV_ESCAPE_MODE_AUTO)
        mode = AV_ESCAPE_MODE_BACKSLASH; /* TODO: implement a heuristic */

    switch (mode) {
    case AV_ESCAPE_MODE_QUOTE:
        /* enclose the string between '' */
       av_bprint_chars(dstbuf, '\'', 1);
       for (; *src; src++) {
            if (*src == '\'')
                av_bprintf(dstbuf, "'\\''");
            else
                av_bprint_chars(dstbuf, *src, 1);
        }
        av_bprint_chars(dstbuf, '\'', 1);
        break;

    /* case AV_ESCAPE_MODE_BACKSLASH or unknown mode */
    default:
        /* \-escape characters */
        for (; *src; src++) {
            int is_first_last       = src == src0 || !*(src+1);
            int is_ws               = !!strchr(WHITESPACES, *src);
            int is_strictly_special = special_chars && strchr(special_chars, *src);
            int is_special          =
                is_strictly_special || strchr("'\\", *src) ||
                (is_ws && (flags & AV_ESCAPE_FLAG_WHITESPACE));

            if (is_strictly_special ||
                (!(flags & AV_ESCAPE_FLAG_STRICT) &&
                (is_special || (is_ws && is_first_last))))
                av_bprint_chars(dstbuf, '\\', 1);
            av_bprint_chars(dstbuf, *src, 1);
        }
        break;
    }
}
#endif