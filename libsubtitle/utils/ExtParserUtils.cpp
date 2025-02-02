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

#define LOG_TAG "ExtParserUtils"

#include "ExtParserUtils.h"
ExtParserUtils::ExtParserUtils(int charset, std::shared_ptr<DataSource> source) {
    subfile_buffer = NULL;
    mDataSource = source;
    mEncoding = charset;

    // if setup is utf8, we do an auto detect.
    if (mEncoding == AML_ENCODING_NONE) {
        detectEncoding();
    }
}

void ExtParserUtils::detectEncoding() {
    if (mDataSource == nullptr) return;

    char first[3];
    mDataSource->lseek(0, SEEK_SET);
    int r = mDataSource->read(first, 3);
    if (r < 3) {
        mDataSource->lseek(0, SEEK_SET);
        return;
    }

    // TODO: utf32 used rarely, we may also need check it.
    if (first[0] == 0xFF && first[1] == 0xFE) {
        mEncoding = AML_ENCODING_UTF16;
    } else if (first[0] == 0xFE && first[1] == 0xFF) {
        mEncoding = AML_ENCODING_UTF16BE;
    } else if (first[0] == 0xEF && first[1] == 0xBB && first[2] == 0xBF) {
        mEncoding = AML_ENCODING_UTF8;
    } else {
        // NO BOM, auto detect, almost all use utf8, we current NONE treat as utf8
    }

    SUBTITLE_LOGI("%x %x %x mEncoding=%d", first[0], first[1], first[2], mEncoding);
    mDataSource->lseek(0, SEEK_SET);
}


ExtParserUtils::~ExtParserUtils() {
    if (subfile_buffer != NULL) {
        free(subfile_buffer);
        subfile_buffer = NULL;
    }
}

int ExtParserUtils::Ext_Subtitle_uni_Utf16toUtf8(int subformat, const UTF16 *in, int inLen, UTF8 *out, int outMax) {
    int outLen = 0;
    if (out) {
        // Output buffer passed in; actually encode data.
        while (inLen > 0) {
            UTF16 ch = *in;
            if (subformat == AML_ENCODING_UTF16) {
                ch = ch << 8 | ch >> 8;
            }
            inLen--;
            if (ch < 0x80) {
                if (--outMax < 0) {
                    return -1;
                }
                *out++ = (UTF8) ch;
                outLen++;
            } else if (ch < 0x800) {
                if ((outMax -= 2) < 0) {
                    return -1;
                }
                *out++ = (UTF8)(0xC0 | ((ch >> 6) & 0x1F));//1
                *out++ = (UTF8)(0x80 | (ch & 0x3F));
                outLen += 2;
            } else if (ch >= 0xD800 && ch <= 0xDBFF) {
                UTF16 ch2;
                UTF32 ucs4;
                if (--inLen < 0) {
                    return -1;
                }
                ch2 = *++in;
                if (ch2 < 0xDC00 || ch2 > 0xDFFF) {
                    // This is an invalid UTF-16 surrogate pair sequence
                    // Encode the replacement character instead
                    ch = 0xFFFD;
                    goto Encode3;
                }
                ucs4 =
                    ((ch - 0xD800) << 10) + (ch2 - 0xDC00) +
                    0x10000;
                if ((outMax -= 4) < 0) {
                    return -1;
                }
                *out++ = (UTF8)(0xF0 | ((ucs4 >> 18) & 0x07));//2
                *out++ = (UTF8)(0x80 | ((ucs4 >> 12) & 0x3F));
                *out++ = (UTF8)(0x80 | ((ucs4 >> 6) & 0x3F));
                *out++ = (UTF8)(0x80 | (ucs4 & 0x3F));
                outLen += 4;
            } else {
                if (ch >= 0xDC00 && ch <= 0xDFFF) {
                    // This is an invalid UTF-16 surrogate pair sequence
                    // Encode the replacement character instead
                    ch = 0xFFFD;
                }
Encode3:
                if ((outMax -= 3) < 0) {
                    return -1;
                }
                *out++ = (UTF8)(0xE0 | ((ch >> 12) & 0x0F));
                *out++ = (UTF8)(0x80 | ((ch >> 6) & 0x3F));
                *out++ = (UTF8)(0x80 | (ch & 0x3F));
                outLen += 3;
            }
            in++;
        }
    } else {
        // Count output characters without actually encoding.
        while (inLen > 0) {
            UTF16 ch = *in, ch2;
            inLen--;
            if (ch < 0x80) {
                outLen++;
            } else if (ch < 0x800) {
                outLen += 2;
            } else if (ch >= 0xD800 && ch <= 0xDBFF) {
                if (--inLen < 0) {
                    return -1;
                }
                ch2 = *++in;
                if (ch2 < 0xDC00 || ch2 > 0xDFFF) {
                    // Invalid...
                    // We'll encode 0xFFFD for this
                    outLen += 3;
                } else {
                    outLen += 4;
                }
            } else {
                outLen += 3;
            }
            in++;
        }
    }
    return outLen;
}

int ExtParserUtils::Ext_subtitle_utf16_utf8(int subformat, char *s, int inLen) {
    UTF8 *utf8_str = NULL;
    int outLen = 0;
    utf8_str = (unsigned char *)MALLOC(inLen * 2 + 1);
    if (utf8_str == NULL)
        return 0;
    memset(utf8_str, 0x0, inLen * 2 + 1);
    outLen = Ext_Subtitle_uni_Utf16toUtf8(subformat, (const UTF16 *)s, inLen / 2, utf8_str, inLen * 2);
    if (outLen > 0) {
        memcpy(s, utf8_str, outLen);
        s[outLen] = '\0';
    } else {
        memset(s, 0x0, inLen);
    }
    FREE(utf8_str);
    return 1;
}


/* internal string manipulation functions */
int ExtParserUtils::ExtSubtitleEol(char p) {
    return (p == '\r' || p == '\n' || p == '\0');
}

char *ExtParserUtils::ExtSubtiltleReadtext(char *source, char **dest) {
    int len = 0;
    char *p = source;
    while (!ExtSubtitleEol(*p) && *p != '|') {
        p++, len++;
    }
    *dest = (char *)MALLOC(len + 1);
    if (!dest) {
        return (char *)ERR;
    }
    strncpy(*dest, source, len);
    (*dest)[len] = 0;
    while (*p == '\r' || *p == '\n' || *p == '|') {
        p++;
    }
    if (*p) {
        /* not-last text field */
        return p;
    } else {
        /* last text field */
        return NULL;
    }
}

char *ExtParserUtils::ExtSubtitleStrdup(char *src) {
    char *ret;
    int len;
    len = strlen(src);
    ret = (char *)MALLOC(len + 1);
    if (ret) {
        strcpy(ret, src);
    }
    return ret;
}

char *ExtParserUtils::ExtSubtitleStristr(const char *haystack, const char *needle) {
    int len = 0;
    const char *p = haystack;
    if (!(haystack && needle)) {
        return NULL;
    }
    len = strlen(needle);
    while (*p != '\0') {
        if (strncasecmp(p, needle, len) == 0)
            return (char *)p;
        p++;
    }
    return NULL;
}

void ExtParserUtils::ExtSubtitleTrailspace(char *s) {
    int i = 0;
    int len = strlen(s) + 1;
    char *r = (char *)malloc(len);
    memset(r, 0, len);

    while (isspace(s[i]))
        ++i;

    strcpy(r, s + i);

    int k = strlen(r) - 1;
    while (k > 0 && isspace(r[k]))
        r[k--] = '\0';

    memcpy(s, r, len);  // avoid strcpy memory overlap warning
    free(r);
}

// TODO: rewrite this part
char *ExtParserUtils::ExtSubtitlesfileGets(char *s/*, int fd*/)
{
    int offset = mFileReaded;
    int copied;
    if (!subfile_buffer) {
        subfile_buffer = (char *)MALLOC(LINE_LEN);
        if (subfile_buffer) {
            mFileReaded = 0;
            offset = 0;
            mBufferSize = mDataSource->read(subfile_buffer, LINE_LEN);
            if (mBufferSize <= 0) {
                return NULL;
            }
        } else {
            return NULL;
        }
    }
    if (mBufferSize <= 0) {
        return NULL;
    }
    while ((offset < mBufferSize)
            &&
            (!((mEncoding == AML_ENCODING_NONE || mEncoding == AML_ENCODING_UTF8)
               && (subfile_buffer[offset] == '\n'))
             && !((mEncoding == AML_ENCODING_UTF16) && (subfile_buffer[offset] == 0)
                  && (subfile_buffer[offset + 1] == 0xd)
                  && (subfile_buffer[offset + 2] == 0)
                  && (subfile_buffer[offset + 3] == 0xa))
             && !((mEncoding == AML_ENCODING_UTF16BE) && (subfile_buffer[offset] == 0xd)
                  && (subfile_buffer[offset + 1] == 0)
                  && (subfile_buffer[offset + 2] == 0xa)
                  && (subfile_buffer[offset + 3] == 0))))
    {
        offset++;
    }
    if (offset < mBufferSize) {
        MEMCPY(s, subfile_buffer + mFileReaded, offset - mFileReaded);
        s[offset - mFileReaded] = '\0';
        if (mEncoding == AML_ENCODING_UTF16 || mEncoding == AML_ENCODING_UTF16BE) {
            //subtitle_utf16_utf8(s, offset - mFileReaded);
            Ext_subtitle_utf16_utf8(mEncoding, s, offset - mFileReaded);
            mFileReaded = offset + 4;
        } else
            mFileReaded = offset + 1;
        if (mFileReaded == LINE_LEN) {
            mFileReaded = 0;
            mBufferSize = mDataSource->read(subfile_buffer, LINE_LEN);
        }
    } else {
        if (mBufferSize < LINE_LEN) {
            /* modify by adam in 09.6.30 for get the last subtitle */
            memcpy(s, subfile_buffer + mFileReaded,
                   offset - mFileReaded);
            s[offset - mFileReaded] = '\0';
            if (mEncoding == AML_ENCODING_UTF16 || mEncoding == AML_ENCODING_UTF16BE)
                //subtitle_utf16_utf8(s, offset - mFileReaded);
                Ext_subtitle_utf16_utf8(mEncoding, s, offset - mFileReaded);
            mBufferSize = 0;
            return s;
        }
        copied = LINE_LEN - mFileReaded;
        MEMCPY(s, subfile_buffer + mFileReaded, copied);
        if (mBufferSize == LINE_LEN) {
            /* refill */
            mBufferSize = mDataSource->read(subfile_buffer, LINE_LEN);
            if (mBufferSize < 0) {
                return NULL;
            }
        }
        offset = 0;
        while ((offset < MIN(LINE_LEN - copied, mBufferSize))
                &&
                (!((mEncoding == AML_ENCODING_NONE || mEncoding == AML_ENCODING_UTF8)
                   && (subfile_buffer[offset] == '\n'))
                 && !((mEncoding == AML_ENCODING_UTF16)
                      && (subfile_buffer[offset] == 0)
                      && (subfile_buffer[offset + 1] == 0xd)
                      && (subfile_buffer[offset + 2] == 0)
                      && (subfile_buffer[offset + 3] == 0xa))
                 && !((mEncoding == AML_ENCODING_UTF16BE)
                      && (subfile_buffer[offset] == 0xd)
                      && (subfile_buffer[offset + 1] == 0)
                      && (subfile_buffer[offset + 2] == 0xa)
                      && (subfile_buffer[offset + 3] == 0))))
        {
            offset++;
        }
        if (((mEncoding == AML_ENCODING_NONE || mEncoding == AML_ENCODING_UTF8)
                && subfile_buffer[offset] == '\n')
                || ((mEncoding == AML_ENCODING_UTF16) && (subfile_buffer[offset] == 0)
                    && (subfile_buffer[offset + 1] == 0xd)
                    && (subfile_buffer[offset + 2] == 0)
                    && (subfile_buffer[offset + 3] == 0xa))
                || ((mEncoding == AML_ENCODING_UTF16BE)
                    && (subfile_buffer[offset] == 0xd)
                    && (subfile_buffer[offset + 1] == 0)
                    && (subfile_buffer[offset + 2] == 0xa)
                    && (subfile_buffer[offset + 3] == 0)))
        {
            if (subfile_buffer[offset] == '\n') {
                mFileReaded = offset + 1;
            } else
                mFileReaded = offset + 4;
            if (offset) {
                memcpy(s + copied, subfile_buffer, offset);
            }
            s[copied + offset + 1] = '\0';
            if (mEncoding == AML_ENCODING_UTF16 || mEncoding == AML_ENCODING_UTF16BE)
                //subtitle_utf16_utf8(s, offset + copied);
                Ext_subtitle_utf16_utf8(mEncoding, s, offset + copied);
        }
        else if (mBufferSize < LINE_LEN) {
            /* end of file w/o terminator "\n" */
            return NULL;
        }
        else {
            s[LINE_LEN + 1] = '\0';
        }
    }
    //add to avoid overflow
    if (strlen(s) >= LINE_LEN) {
        s = NULL;
     }
    return s;
}


