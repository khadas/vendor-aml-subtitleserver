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

#ifndef _EXT_PARSER_UTILS_H_
#define _EXT_PARSER_UTILS_H_

#include <unistd.h>
#include <fcntl.h>
#include <malloc.h>
#include <string>
#include "list.h"
#include "DataSource.h"

#define  MALLOC(s)      malloc(s)
#define  FREE(d)        free(d)
#define  MEMCPY(d,s,l)  memcpy(d,s,l)
#define  MEMSET(d,s,l)  memset(d,s,l)
#define  MIN(x,y)       ((x)<(y)?(x):(y))
#define  UTF8           unsigned char
#define  UTF16          unsigned short
#define  UTF32          unsigned int
#define ERR ((void *) -1)
#define LINE_LEN                    1024*5

enum {
    AML_ENCODING_NONE = 0,
    AML_ENCODING_UTF8,
    AML_ENCODING_UTF16,
    AML_ENCODING_UTF16BE,
    AML_ENCODING_UTF32,
    AML_ENCODING_UTF32BE
};

class ExtParserUtils {

public:
    ExtParserUtils(int charset, std::shared_ptr<DataSource> source);
    ~ExtParserUtils();
    int Ext_Subtitle_uni_Utf16toUtf8(int subformat, const UTF16 *in, int inLen, UTF8 *out, int outMax);
    int Ext_subtitle_utf16_utf8(int subformat, char *s, int inLen);
    int ExtSubtitleEol(char p);
    char *ExtSubtiltleReadtext(char *source, char **dest);
    char *ExtSubtitleStrdup(char *src);
    char *ExtSubtitleStristr(const char *haystack, const char *needle);
    void ExtSubtitleTrailspace(char *s) ;
    char *ExtSubtitlesfileGets(char *s/*, int fd*/);

private:
    void detectEncoding();

    char *subfile_buffer;
    int mBufferSize;
    unsigned mFileReaded;
    std::shared_ptr<DataSource> mDataSource;
    int mEncoding;
};

#endif

