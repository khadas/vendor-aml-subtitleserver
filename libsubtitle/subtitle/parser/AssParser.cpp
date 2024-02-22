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

#define LOG_TAG "AssParser"
#include <unistd.h>
#include <fcntl.h>

#include<iostream>
#include <sstream>
#include <string>
#include <list>
#include <thread>
#include <algorithm>
#include <functional>

#include "SubtitleLog.h"

#include "SubtitleTypes.h"
#include "AssParser.h"
#include "ParserFactory.h"

#include "StreamUtils.h"
#define MIN_HEADER_DATA_SIZE 24

static inline std::string stringConvert2Stream(std::string s1, std::string s2) {
    std::stringstream stream;
    if (s1.length() == 0 || s1 == "") {
        stream << s2;
    } else {
        stream << s1;
        stream << "\n";
        stream << s2;
    }
    return stream.str();
}

/* param s the string below have double language text.
 *     Dialogue: ,0:01:25.16,0:01:26.72,*456,1,0000,0000,0000,,Hey! Come here!
 *     Dialogue: ,0:01:25.16,0:01:26.72,*123,1,0000,0000,0000.
 * return string for second line language text
 *
*/
static inline std::string getSecondTextForDoubleLanguage(std::string source) {
    std::size_t indexOfLineBreak;
    std::stringstream secondStream;
    indexOfLineBreak = source.find("\n");
    if (indexOfLineBreak == std::string::npos) {
        SUBTITLE_LOGE("NO double language, return");
        return "";
    }
    std::string secondStr;
    secondStr = source.substr(indexOfLineBreak + 1, source.length());

    const int ASS_EVENT_SECTIONS = 9;
    const int BUILTIN_ASS_EVENT_SECTIONS = 8;
    std::stringstream ss;
    std::string str;
    std::vector<std::string> items; // store the event sections in vector
    bool isNormal = strncmp((const char *)secondStr.c_str(), "Dialogue:", 9) == 0;
    ss << secondStr;

    int i = 0;
    while (getline(ss, str, ',')) {
        i++;
        items.push_back(str);
        // keep the last subtitle content not split with ',', the content may has it.
        if (isNormal && i == ASS_EVENT_SECTIONS) {
            break;
        } else if (!isNormal && i == BUILTIN_ASS_EVENT_SECTIONS) {
            break;
        }
      }
      std::string tempStr = ss.str();
      //fist check the "{\" in the content. If don't find ,just use getline to get the content.
      int nPos = tempStr.find("{\\");
      if (-1 == nPos) {
          getline(ss, str);
      } else {
          str = tempStr.substr(nPos, tempStr.length());
      }
      return str;

}


//TODO: move to utils directory

/**
 *  return ascii printed literal value
 */

/*
    retrieve the subtitle content from the decoded buffer.
    If is the origin ass event buffer, also fill the time stamp info.

    normal ASS type:
    Format: Layer, Start, End, Style, Actor, MarginL, MarginR, MarginV, Effect, Text
    Dialogue: 0,0:00:00.26,0:00:01.89,Default,,0000,0000,0000,,Don't move

    ssa type and example:
    Format: Marked, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
    Dialogue: Marked=0,0:00:00.26,0:00:01.89,Default,NTP,0000,0000,0000,!Effect,Don't move

    not normal, typically, built in:
    36,0,Default,,0000,0000,0000,,They'll be back from Johns Hopkins...


   here, we only care about the last subtitle content. other feature, like effect and margin, not support

*/
static inline int __getAssSpu(uint8_t*spuBuf, uint32_t length, std::shared_ptr<AML_SPUVAR> spu) {
    const int ASS_EVENT_SECTIONS = 9;
    // *,0,Default,,0000,0000,0000,,      "," num:8
    const int BUILTIN_ASS_EVENT_SECTIONS = 8;
    std::stringstream ss;
    std::string str;
    std::vector<std::string> items; // store the event sections in vector

    ss << spuBuf;

    // if has dialog, is normal, or is not.
    bool isNormal = strncmp((const char *)spuBuf, "Dialogue:", 9) == 0;

    int i = 0;
    while (getline(ss, str, ',')) {
        i++;
        items.push_back(str);

        // keep the last subtitle content not split with ',', the content may has it.
        if (isNormal && i == ASS_EVENT_SECTIONS) {
             break;
        } else if (!isNormal && i == BUILTIN_ASS_EVENT_SECTIONS) {
            break;
        }
    }

    // obviously, invalid dialog from the spec.
    if (i < BUILTIN_ASS_EVENT_SECTIONS) return -1;

    if (isNormal) {
        uint32_t hour, min, sec, ms;
        int count = 0;

        // 1 is start time
        count = sscanf(items[1].c_str(), "%d:%d:%d.%d", &hour, &min, &sec, &ms);
        if (count == 4) {
            spu->pts = (hour * 60 * 60 + min * 60 + sec) * 1000 + ms * 10;
            spu->pts *= 90;
        }

        // 2 is end time.
        count = sscanf(items[2].c_str(), "%d:%d:%d.%d", &hour, &min, &sec, &ms);
        if (count == 4) {
            spu->m_delay = (hour * 60 * 60 + min * 60 + sec) * 1000 + ms * 10;
            spu->m_delay *= 90;
        }
    }

    // get the subtitle content. here we do not need other effect and margin data.
    std::string tempStr = ss.str();
    //fist check the "{\" in the content. If don't find ,just use getline to get the content.
    int nPos = tempStr.find("{\\");
    if (-1 == nPos) {
        getline(ss, str);
    } else {
        str = tempStr.substr(nPos, tempStr.length());
    }
    SUBTITLE_LOGI("[%s]-subtitle=%s", ss.str().c_str(), str.c_str());
    // currently not support style control code rendering
    // discard the unsupported {} Style Override control codes
    std::size_t start, end;
    while ((start = str.find("{\\")) != std::string::npos) {
        end = str.find('}', start);
        if (end != std::string::npos) {
            str.erase(start, end-start+1);
        } else {
            break;
        }
    }
    // replace "\n"
    while ((start = str.find("\\n")) != std::string::npos
            || (start = str.find("\\N")) != std::string::npos) {
        std::string newline = "\n";
        str.replace(start, 2, newline);
    }

    std::string secondStr = getSecondTextForDoubleLanguage(tempStr);
    str = stringConvert2Stream(secondStr, str);

    strcpy((char *)spu->spu_data, str.c_str());
    return 0;
}

AssParser::AssParser(std::shared_ptr<DataSource> source) {
    mDataSource = source;
    mParseType = TYPE_SUBTITLE_SSA;
    mRestLen = 0;
    mRestbuf = nullptr;
}

int AssParser::getSpu(std::shared_ptr<AML_SPUVAR> spu) {
    int ret = -1;
    char *spuBuf = NULL;
    int64_t currentPts;
    uint32_t currentLen, currentType, durationPts;

    if (mState == SUB_INIT) {
        mState = SUB_PLAYING;
    } else if (mState == SUB_STOP) {
        SUBTITLE_LOGI(" subtitle_status == SUB_STOP \n\n");
        return 0;
    }

    int dataSize = mDataSource->availableDataSize();
    if (dataSize <= 0) {
        return -1;
    } else {
        dataSize += mRestLen;
        currentType = 0;
        spuBuf = new char[dataSize]();
    }


    // Got enough data (MIN_HEADER_DATA_SIZE bytes), then start parse
    while (dataSize >= MIN_HEADER_DATA_SIZE) {
        SUBTITLE_LOGI("dataSize =%u  mRestLen=%d,", dataSize, mRestLen);

        char *tmpSpuBuffer = spuBuf;
        char *spuBufPiece = tmpSpuBuffer;
        if (mRestLen) {
            memcpy(spuBufPiece, mRestbuf, mRestLen);
        }

        //for coverity dead error condition
        mDataSource->read(spuBufPiece + mRestLen, 20);
        dataSize -= 20;
        tmpSpuBuffer += 20;

        int rdOffset = 0;
        int syncWord = subPeekAsInt32(spuBufPiece + rdOffset);
        rdOffset += 4;
        if (syncWord != AML_PARSER_SYNC_WORD) {
            SUBTITLE_LOGE("\n wrong subtitle header :%x %x %x %x    %x %x %x %x    %x %x %x %x \n",
                    spuBufPiece[0], spuBufPiece[1], spuBufPiece[2], spuBufPiece[3],
                    spuBufPiece[4], spuBufPiece[5], spuBufPiece[6], spuBufPiece[7],
                    spuBufPiece[8], spuBufPiece[9], spuBufPiece[10], spuBufPiece[11]);
            mDataSource->read(spuBufPiece, dataSize);
            //dataSize = 0;
            SUBTITLE_LOGE("\n\n ******* find wrong subtitle header!! ******\n\n");
            delete[] spuBuf;
            return -1;

        }

        SUBTITLE_LOGI("\n\n ******* find correct subtitle header ******\n\n");
        // ignore first sync byte: 0xAA/0x77
        currentType = subPeekAsInt32(spuBufPiece + rdOffset) & 0x00FFFFFF;
        rdOffset += 4;
        currentLen = subPeekAsInt32(spuBufPiece + rdOffset);
        rdOffset += 4;
        currentPts = subPeekAsInt64(spuBufPiece + rdOffset);
        rdOffset += 8;
        SUBTITLE_LOGI("dataSize=%u, currentType:%x, currentPts is %llx, currentLen is %d, \n",
                dataSize, currentType, currentPts, currentLen);
        if (currentLen > dataSize) {
            SUBTITLE_LOGI("currentLen > size");
            mDataSource->read(spuBufPiece, dataSize);
            //dataSize = 0;
            delete[] spuBuf;
            return -1;
        }

        // TODO: types move to a common header file, do not use magic number.
        if (currentType == AV_CODEC_ID_DVD_SUBTITLE || currentType == AV_CODEC_ID_VOB_SUBTITLE) {
            mDataSource->read(spuBufPiece + mRestLen + 20, dataSize - mRestLen);
            mRestLen = dataSize;
            dataSize = 0;
            tmpSpuBuffer += currentLen;
            SUBTITLE_LOGI("currentType=0x17000 or 0x1700a! mRestLen=%d, dataSize=%d,\n", mRestLen, dataSize);
        } else {
            mDataSource->read(spuBufPiece + 20, currentLen + 4);
            dataSize -= (currentLen + 4);
            tmpSpuBuffer += (currentLen + 4);
            mRestLen = 0;
        }


        switch (currentType) {
            case AV_CODEC_ID_VOB_SUBTITLE:   //mkv internal image
                durationPts = subPeekAsInt32(spuBufPiece + rdOffset);
                rdOffset += 4;
                mRestLen -= 4;
                SUBTITLE_LOGI("durationPts is %d\n", durationPts);
                break;

            case AV_CODEC_ID_TEXT:   //mkv internal utf-8
            case AV_CODEC_ID_SSA:   //mkv internal ssa
            case AV_CODEC_ID_SUBRIP:   //mkv internal SUBRIP
            case AV_CODEC_ID_ASS:   //mkv internal ass
            case AV_CODEC_ID_WEBVTT:
                durationPts = subPeekAsInt32(spuBufPiece + rdOffset);
                rdOffset += 4;
                spu->subtitle_type = TYPE_SUBTITLE_SSA;
                spu->buffer_size = currentLen + 1;  //256*(currentLen/256+1);
                spu->spu_data = new uint8_t[spu->buffer_size]();
                spu->pts = currentPts;
                spu->m_delay = durationPts;
                if (durationPts != 0) {
                    spu->m_delay += currentPts;
                }

                memcpy(spu->spu_data, spuBufPiece + rdOffset, currentLen);
                if (currentType == AV_CODEC_ID_SSA || currentType == AV_CODEC_ID_ASS) {
                    ret = __getAssSpu(spu->spu_data, spu->buffer_size, spu);
                    SUBTITLE_LOGI("CODEC_ID_SSA  size is:%u ,data is:%s, currentLen=%d\n",
                             spu->buffer_size, spu->spu_data, currentLen);
                } else if (currentType == AV_CODEC_ID_SUBRIP) {
                    if (currentLen >= 7) {
                        //Check if the beginning is "<i>" and the end is marked in italics "</i>"
                        if (memcmp(spu->spu_data, "<i>", 3) == 0 && memcmp(spu->spu_data + currentLen - 4, "</i>", 4) == 0) {
                            memmove(spu->spu_data, spu->spu_data + 3, currentLen - 7); //Remove the starting "<i>"
                            spu->spu_data[currentLen - 7] = '\0'; //Remove "</i>" from the tail
                        }
                    }
                    SUBTITLE_LOGI("CODEC_ID_SUBRIP  size is:%u ,data is:%s, currentLen=%d\n", spu->buffer_size, spu->spu_data, currentLen);
                    ret = 0;
                } else {
                    ret = 0;
                }

                break;

            case AV_CODEC_ID_MOV_TEXT:
                durationPts = subPeekAsInt32(spuBufPiece + rdOffset);
                rdOffset += 4;
                spu->subtitle_type = TYPE_SUBTITLE_TMD_TXT;
                spu->buffer_size = currentLen + 1;
                spu->spu_data = new uint8_t[spu->buffer_size]();
                spu->pts = currentPts;
                spu->m_delay = durationPts;
                if (durationPts != 0) {
                    spu->m_delay += currentPts;
                }
                rdOffset += 2;
                currentLen -= 2;
                if (currentLen == 0) {
                    ret = -1;
                    break;
                }
                memcpy(spu->spu_data, spuBufPiece + rdOffset, currentLen);
                SUBTITLE_LOGI("CODEC_ID_TIME_TEXT   size is:    %u ,data is:    %s, currentLen=%d\n",
                        spu->buffer_size, spu->spu_data, currentLen);
                ret = 0;
                break;

            default:
                SUBTITLE_LOGI("received invalid type %x", currentType);
                ret = -1;
                break;
        }

        if (ret < 0) break;

        //std::list<std::shared_ptr<AML_SPUVAR>> mDecodedSpu;
        // TODO: add protect? only list operation may no need.
        // TODO: sort
         addDecodedItem(std::shared_ptr<AML_SPUVAR>(spu));
         //every time only parse one package, otherwise will cover
         //last frame data.
         break;
    }

    //SUBTITLE_LOGI("[%s::%d] error! spuBuf=%x, \n", __FUNCTION__, __LINE__, spuBuf);
    if (spuBuf) {
        delete[] spuBuf;
        //spuBuf = NULL;
    }
    return ret;
}


int AssParser::getInterSpu() {
    std::shared_ptr<AML_SPUVAR> spu(new AML_SPUVAR());

    //TODO: common place
    spu->sync_bytes = AML_PARSER_SYNC_WORD;//0x414d4c55;
    // simply, use new instead of malloc, can automatically initialize the buffer
    spu->useMalloc = false;
    spu->isSimpleText = true;
    int ret = getSpu(spu);

    return ret;
}


int AssParser::parse() {
    while (!mThreadExitRequested) {
        if (getInterSpu() < 0) {
            // advance input and parse failed, wait and retry.
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
        }
    }
    return 0;
}

void AssParser::dump(int fd, const char *prefix) {
    dprintf(fd, "%s ASS Parser\n", prefix);
    dumpCommon(fd, prefix);
    dprintf(fd, "%s  rest Length=%d\n", prefix, mRestLen);

}



