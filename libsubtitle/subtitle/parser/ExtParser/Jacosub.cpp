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

#define LOG_TAG "Jacosub"
#include <regex>

#include "Jacosub.h"


static inline void removeComments(std::string &s) {
    std::regex r("(\\{([^}])*\\})");
    bool replaced;
    do {
        replaced = false;
        std::sregex_iterator next(s.begin(), s.end(), r);
        std::sregex_iterator end;
        if (next != end) {
            std::smatch match = *next;
            s.erase(s.find(match.str()), match.str().length());
            replaced = true;
        }
    } while (replaced);
}


Jacosub::Jacosub(std::shared_ptr<DataSource> source): TextSubtitle(source) {
    SUBTITLE_LOGI("Jacosub");
    mShift = 0;
    mTimerRes = 30;
    parseHeaderInfo();
    bool r = mReader->rewindStream();

    SUBTITLE_LOGI("Jacosub mShift=%d mTimerRes=%d rewind:%d", mShift, mTimerRes, r);
}

Jacosub::~Jacosub() {

}

void Jacosub::parseHeaderInfo() {
    char line[LINE_LEN+1];

    while (mReader->getLine(line)) {
        char *p = triml(line, "\t ");
        int hours = 0, minutes = 0, seconds = 0;
        unsigned units = mShift;
        int delta = 0, inverter = 1;
        if (line[0] == '#') {
            switch (toupper(line[1])) {
                case 'S': {
                    if (isalpha(line[2])) {
                        delta = 6;
                    } else {
                        delta = 2;
                    }
                    if (sscanf(&line[delta], "%d", &hours)) {
                        if (hours < 0) {
                            hours *= -1;
                            inverter = -1;
                        }
                        if (sscanf(&line[delta], "%*d:%d", &minutes)) {
                            if (sscanf(&line[delta], "%*d:%*d:%d", &seconds)) {
                                sscanf(&line[delta], "%*d:%*d:%*d.%d", &units);
                            } else {
                                hours = 0;
                                sscanf(&line[delta], "%d:%d.%d", &minutes, &seconds, &units);
                                minutes *= inverter;
                            }
                        } else {
                            hours = minutes = 0;
                            sscanf(&line[delta], "%d.%d", &seconds, &units);
                            seconds *= inverter;
                        }
                        mShift = ((hours * 3600 + minutes * 60 + seconds) * mTimerRes + units) * inverter;
                    }
                }
                break;

                case 'T': {
                    if (isalpha(line[2])) {
                        delta = 8;
                    } else {
                        delta = 2;
                    }
                    sscanf(&line[delta], "%u", &mTimerRes);
                }
                break;

                default:
                    break;
            }
        }else {//this is need, or else will no subtitle show
            break;
        }
    }

}

//
//  Japanese Animation Club of Orlando
// official size: http://unicorn.us.com/jacosub/
// deprecated, spec cannot access now
//
std::shared_ptr<ExtSubItem> Jacosub::decodedItem() {
    char *p, *q;
    char *line1 = (char *)MALLOC(LINE_LEN);
    if (!line1) {
        SUBTITLE_LOGE("[%s::%d]line1 malloc error!\n", __FUNCTION__, __LINE__);
        return nullptr;
    }
    char *line2 = (char *)MALLOC(LINE_LEN);
    if (!line2) {
        free(line1);
        SUBTITLE_LOGE("[%s::%d]line2 malloc error!\n", __FUNCTION__, __LINE__);
        return nullptr;
    }
    char *directive = (char *)MALLOC(LINE_LEN);
    if (!directive) {
        SUBTITLE_LOGE("[%s::%d] directive malloc error!\n", __FUNCTION__, __LINE__);
        free(line1);
        free(line2);
        return nullptr;
    }
    memset(line1, 0, LINE_LEN);
    memset(line2, 0, LINE_LEN);
    memset(directive, 0, LINE_LEN);

    // TODO: C++ style
    while (mReader->getLine(line1)) {
        unsigned a1, a2, a3, a4, b1, b2, b3, b4;
        unsigned long start, end;

        if (sscanf(line1, "%u:%u:%u.%u %u:%u:%u.%u %[^\n\r]",
                   &a1, &a2, &a3, &a4, &b1, &b2, &b3, &b4, line2) < 9) {
            if (sscanf(line1, "@%u @%u %[^\n\r]", &a4, &b4, line2) <3) {
                continue;
            }
            start = (unsigned long)((a4 + mShift) * 100.0 / mTimerRes);
            end = (unsigned long)((b4 + mShift) * 100.0 / mTimerRes);
        } else {
            start = (unsigned long)(((a1 * 3600 + a2 * 60 + a3) * mTimerRes + a4 + mShift) * 100.0 / mTimerRes);
            end = (unsigned long)(((b1 * 3600 + b2 * 60 + b3) * mTimerRes + b4 + mShift) * 100.0 / mTimerRes);
        }

        char *p = triml(line2, "\t ");

        // parse directives.
        if (isalpha(*p) || *p == '[') {
            int cont, jLength;
            if (sscanf(p, "%s %[^\n\r]", directive, line1) < 2) {
                continue;
            }
            jLength = strlen(directive);

            for (cont = 0; cont < jLength; ++cont) {
                if (isalpha(*(directive + cont))) {
                    *(directive + cont) = toupper(*(directive + cont));
                }
            }

            if ((strstr(directive, "RDB") != NULL)
                    || (strstr(directive, "RDC") != NULL)
                    || (strstr(directive, "RLB") != NULL)
                    || (strstr(directive, "RLG") != NULL)) {
                continue;
            }
            if (strstr(directive, "JL") != NULL) {
                //alignment = SUB_ALIGNMENT_BOTTOMLEFT;
            }
            else if (strstr(directive, "JR") != NULL)
            {
                //alignment = SUB_ALIGNMENT_BOTTOMRIGHT;
            } else {
                //alignment = SUB_ALIGNMENT_BOTTOMCENTER;
            }
            strcpy(line2, line1);
            p = line2;
        }


        // parse string, TODO: change to C++ style
        std::string subStr;
        std::string s(p);
        removeComments(s);
        strcpy(p, s.c_str());

        for (q = line1; !mReader->ExtSubtitleEol(*p); ++p) {
            unsigned int comment = 0;
            switch (*p) {
                case '{':
/*                    comment++;
                    break;
                case '}':
                    if (comment) {
                        --comment;
                        //the next line to get rid of a blank after the comment
                        if ((*(p + 1)) == ' ')
                            p++;
                    }
                    break;
*/                case '~':
                    if (!comment) {
                        *q = ' ';
                        ++q;
                    }
                    break;
                case ' ':
                case '\t':
                    if ((*(p + 1) == ' ') || (*(p + 1) == '\t')) {
                        break;
                    }
                    if (!comment) {
                        *q = ' ';
                        ++q;
                    }
                    break;
                case '\\':
                    if (*(p + 1) == 'n') {
                        *q = '\0';
                        q = line1;
                        subStr.append(line1);
                        subStr.append("\n");
                        ++p;
                        break;
                    }
                    if ((toupper(*(p + 1)) == 'C') || (toupper(*(p + 1)) == 'F')) {
                        ++p, ++p;
                        break;
                    }
                    if ((*(p + 1) == 'B') || (*(p + 1) == 'b') || (*(p + 1) == 'D') ||  //actually this means "insert current date here"
                            (*(p + 1) == 'I') || (*(p + 1) == 'i') || (*(p + 1) == 'N') || (*(p + 1) == 'T') || //actually this means "insert current time here"
                            (*(p + 1) == 'U') || (*(p + 1) == 'u')) {
                        ++p;
                        break;
                    }
                    if ((*(p + 1) == '\\') || (*(p + 1) == '~') || (*(p + 1) == '{')) {
                        ++p;
                    } else if (mReader->ExtSubtitleEol(*(p + 1))) {
                        if (!mReader->getLine(directive)) {
                            free(line1);
                            free(line2);
                            free(directive);
                            return nullptr;
                        }
                        mReader->trimSpace(directive);
                        strncat(line2, directive, (LINE_LEN > 511) ? (LINE_LEN - 1) : 511);
                        break;
                    }
                    [[fallthrough]];
                default:
                    if (!comment) {
                        *q = *p;
                        ++q;
                    }
                    break;
            }   //-- switch
        }
        *q = '\0';
        char *dupLine1 = mReader->strdup(line1);
        subStr.append(dupLine1);
        std::shared_ptr<ExtSubItem> item = std::shared_ptr<ExtSubItem>(new ExtSubItem());
        item->start = start;
        item->end = end;
        item->lines.push_back(subStr);
        free(line1);
        free(line2);
        free(directive);
        if (dupLine1) free(dupLine1);
        return item;
    }
    free(line1);
    free(line2);
    free(directive);
    return nullptr;
}

