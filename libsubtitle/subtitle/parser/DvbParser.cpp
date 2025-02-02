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

#define LOG_TAG "DvbParser"
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <list>
#include <thread>
#include <algorithm>
#include <functional>

#include <utils/CallStack.h>
#include "SubtitleLog.h"

#include "StreamUtils.h"
#include "SubtitleTypes.h"

#include "DvbParser.h"
#include "ParserFactory.h"

struct DVBSubCLUT {
    int id;
    uint32_t clut4[4];
    uint32_t clut16[16];
    uint32_t clut256[256];

    struct DVBSubCLUT *next;
};


struct DVBSubObjectDisplay {
    int objectId;
    int regionId;

    int xPos;
    int yPos;

    int fgcolor;
    int bgcolor;

    struct DVBSubObjectDisplay *regionListNext;
    struct DVBSubObjectDisplay *objectListNext;
    void dump(int fd, const char *prefix) {
        dprintf(fd, "%s   DVBSubObjectDisplay:%p\n", prefix, this);
        dprintf(fd, "%s     objectId=%d regionId=%d xPos=%d yPos=%d fgcolor=%x bgcolor=%x\n",
            prefix, objectId, regionId, xPos, yPos, fgcolor, bgcolor);

        struct DVBSubObjectDisplay *r = regionListNext;
        dprintf(fd, "%s     regionListNext:%p\n", prefix, r);
        while (r != nullptr) {
             r->dump(fd, prefix);
        }
        struct DVBSubObjectDisplay *o = objectListNext;
        dprintf(fd, "%s     objectListNext:%p\n", prefix, o);
        while (o != nullptr) {
             o->dump(fd, prefix);
        }

    }
};

struct DVBSubObject {
    int id;
    int type;
    DVBSubObjectDisplay *displayList;
    struct DVBSubObject *next;
};

struct DVBSubRegionDisplay {
    int regionId;

    int xPos;
    int yPos;

    struct DVBSubRegionDisplay *next;
};

struct DVBSubRegion {
    int id;

    int width;
    int height;
    int depth;

    int clut;
    int bgcolor;

    uint8_t *pbuf;
    int bufSize;

    DVBSubObjectDisplay *displayList;

    struct DVBSubRegion *next;
};

struct DVBSubDisplayDefinition {
    int version;
    int x;
    int y;
    int width;
    int height;
};

struct DVBSubContext {

    int compositionId;
    int ancillaryId;

    unsigned int lastSpuOriginDisplayW;
    unsigned int lastSpuOriginDisplayH;

    int64_t timeOut;
    DVBSubRegion *regionList;
    DVBSubCLUT *clutList;
    DVBSubObject *objectList;

    int displayListSize;
    DVBSubRegionDisplay *displayList;
    DVBSubDisplayDefinition *displayDefinition;
    DVBSubContext() : compositionId(0), ancillaryId(0), timeOut(0),
            regionList(nullptr), clutList(nullptr), objectList(nullptr),
            displayListSize(0), displayList(nullptr), displayDefinition(nullptr)
    {
        SUBTITLE_LOGI("%s %d", __func__, __LINE__);
        lastSpuOriginDisplayW = 0;
        lastSpuOriginDisplayH = 0;
    }
    ~DVBSubContext() {SUBTITLE_LOGI("%s %d", __func__, __LINE__);}

    void dump(int fd, const char *prefix) {
        dprintf(fd, "\n");
        dprintf(fd, "%s compositionId(%d) ancillaryId(%d) displayListSize(%d)\n",
            prefix, compositionId, ancillaryId, displayListSize);

        dprintf(fd, "\n");
        dprintf(fd, "%s Display Definition=%p\n", prefix, displayDefinition);
        if (displayDefinition != nullptr) {
            dprintf(fd, "%s   version=%d Display[%d %d %d %d]\n", prefix, displayDefinition->version,
                displayDefinition->x, displayDefinition->y,
                displayDefinition->width, displayDefinition->height);
        }

        dprintf(fd, "\n");
        dprintf(fd, "%s Region List=%p\n", prefix, regionList);
        DVBSubRegion *r = regionList;
        while (r != nullptr) {
            dprintf(fd, "%s   id=%d (%dx%d) depth=%d ", prefix, r->id, r->width, r->height, r->depth);
            dprintf(fd, "clut=%d bgcolor=%x buffer=%p size=%d\n", r->clut, r->bgcolor, r->pbuf, r->bufSize);
            r = r->next;
        }

        dprintf(fd, "\n");
        dprintf(fd, "%s Object List=%p\n", prefix, objectList);
        DVBSubObject *o = objectList;
        while (o != nullptr) {
            dprintf(fd, "%s   id=%d type=%d DisplayList=%p \n", prefix, o->id, o->type, o->displayList);
            // TODO: show display
            DVBSubObjectDisplay *_d = o->displayList;
            if (_d != nullptr) {
                _d->dump(fd, prefix);
                //dprintf(fd, "%s        DisplayList ==> regionId=%d xPos=%d yPos=%d\n",
                //        prefix, _d->regionId, _d->xPos, _d->yPos);
                //_d = _d->next;
            }
            o = o->next;
        }

        dprintf(fd, "\n");
        dprintf(fd, "%s Display List(Sub Region)=%p\n", prefix, displayList);
        DVBSubRegionDisplay *d = displayList;
        while (d != nullptr) {
            dprintf(fd, "%s   regionId=%d xPos=%d yPos=%d\n", prefix, d->regionId, d->xPos, d->yPos);
            d = d->next;
        }
        dprintf(fd, "\n");
    }
};

#define OSD_HALF_SIZE (1920*1280/8)

/* define for segment type */
#define DVBSUB_PAGE_SEGMENT                         0x10
#define DVBSUB_REGION_SEGMENT                       0x11
#define DVBSUB_CLUT_SEGMENT                         0x12
#define DVBSUB_OBJECT_SEGMENT                       0x13
#define DVBSUB_DISPLAYDEFINITION_SEGMENT            0x14
#define DVBSUB_DISPLAY_SEGMENT                      0x80
#define DVBSUB_ST_STUFFING                          0xff

/* define for object type */
#define DVBSUB_OT_BASIC_BITMAP                      (0x00)
#define DVBSUB_OT_BASIC_CHARACTER                   (0x01)
#define DVBSUB_OT_COMPOSITE_STRING                  (0x02)

/* define for page composition segment state */
#define DVBSUB_PCS_STATE_NORMAL_CASE                (0x00)
#define DVBSUB_PCS_STATE_ACQUISITION                (0x01)
#define DVBSUB_PCS_STATE_MODE_CHANGE                (0x02)

/* define for pixel data sub block */
#define DVBSUB_DT_2BP_CODE_STRING                   0x10
#define DVBSUB_DT_4BP_CODE_STRING                   0x11
#define DVBSUB_DT_8BP_CODE_STRING                   0x12
#define DVBSUB_DT_24_MAP_TABLE_DATA                 0x20
#define DVBSUB_DT_28_MAP_TABLE_DATA                 0x21
#define DVBSUB_DT_48_MAP_TABLE_DATA                 0x22
#define DVBSUB_DT_END_OF_OBJECT_LINE                0xf0

/*
#define SINGLE_OBJECT_DATA        4
#define SINGLE_FIRST_OBJECT_DATA  1
#define REGION_SEGMENT_CNT        2
*/

#define TOP_FIELD           0
#define BOTTOM_FIELD        1

/* pixel operations */

#define DVB_TIME_OUT_LONG_DURATION  30
#define DVB_TIME_OUT_ADJUST         5

#define HIGH_32_BIT_PTS 0xFFFFFFFF
#define TSYNC_32_BIT_PTS 0xFFFFFFFF


/* crop table */
static const int MAX_NEG_CROP = 1024;
#define __TIME4(x) x, x, x, x
static const uint8_t sffCropTbl[256 + 2 * 1024] = {
    __TIME4(__TIME4(__TIME4(__TIME4(__TIME4(0x00))))),
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
    0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
    0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
    __TIME4(__TIME4(__TIME4(__TIME4(__TIME4(0xFF)))))
};
#define cm (sffCropTbl + MAX_NEG_CROP)

#define SCALEBITS 10
#define ONE_HALF  (1 << (SCALEBITS - 1))
#define FIX(x)    ((int) ((x) * (1<<SCALEBITS) + 0.5))

//TODO: inline functions.
#define YUV_TO_RGB1_CCIR(cb1, cr1)\
    {\
        cb = (cb1) - 128;\
        cr = (cr1) - 128;\
        r_add = FIX(1.40200*255.0/224.0) * cr + ONE_HALF;\
        g_add = - FIX(0.34414*255.0/224.0) * cb - FIX(0.71414*255.0/224.0) * cr + \
                ONE_HALF;\
        b_add = FIX(1.77200*255.0/224.0) * cb + ONE_HALF;\
    }

#define YUV_TO_RGB2_CCIR(r, g, b, y1)\
    {\
        y = ((y1) - 16) * FIX(255.0/219.0);\
        r = cm[(y + r_add) >> SCALEBITS];\
        g = cm[(y + g_add) >> SCALEBITS];\
        b = cm[(y + b_add) >> SCALEBITS];\
    }



static void save2BitmapFile(const char *filename, uint32_t *bitmap, int w, int h) {
    FILE *f;
    char fname[40];
    snprintf(fname, sizeof(fname), "%s.ppm", filename);
    f = fopen(fname, "w");
    if (!f) {
        SUBTITLE_LOGE("Error cannot open file %s!", fname);
        return;
    }
    fprintf(f, "P6\n" "%d %d\n" "%d\n", w, h, 255);
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



#define RGBA(r,g,b,a) (((a) << 24) | ((r) << 16) | ((g) << 8) | (b))

static DVBSubCLUT gDefaultClut;


static inline DVBSubObject *getObject(DVBSubContext *ctx, int objectId) {
    DVBSubObject *ptr = ctx->objectList;
    while (ptr && ptr->id != objectId) {
        ptr = ptr->next;
    }
    return ptr;
}

static inline DVBSubCLUT *getClut(DVBSubContext *ctx, int clutId) {
    DVBSubCLUT *ptr = ctx->clutList;
    while (ptr && ptr->id != clutId) {
        ptr = ptr->next;
    }
    return ptr;
}

static inline DVBSubRegion *getRegion(DVBSubContext *ctx, int regionId) {
    DVBSubRegion *ptr = ctx->regionList;
    while (ptr && ptr->id != regionId) {
        ptr = ptr->next;
    }
    return ptr;
}

static void deleteRegionDisplayList(DVBSubContext *ctx, DVBSubRegion *region) {
    DVBSubObject *object, *obj2, **obj2Ptr;
    DVBSubObjectDisplay *display, *displayObject, **dispObjectPtr;
    while (region->displayList) {
        display = region->displayList;
        object = getObject(ctx, display->objectId);
        if (object) {
            dispObjectPtr = &object->displayList;
            displayObject = *dispObjectPtr;
            while (displayObject && displayObject != display) {
                dispObjectPtr = &displayObject->objectListNext;
                displayObject = *dispObjectPtr;
            }

            if (displayObject) {
                *dispObjectPtr = displayObject->objectListNext;
                if (!object->displayList) {
                    obj2Ptr = &ctx->objectList;
                    obj2 = *obj2Ptr;
                    while (obj2 != object) {
                        assert(obj2);
                        obj2Ptr = &obj2->next;
                        obj2 = *obj2Ptr;
                    }
                    if (obj2) {
                        *obj2Ptr = obj2->next;
                        SUBTITLE_LOGI("@@[%s::%d]free ptr=%p\n", __FUNCTION__, __LINE__, obj2);
                        free(obj2);
                        obj2 = NULL;
                    }
                }
            }
        }
        region->displayList = display->regionListNext;
        if (display) {
            SUBTITLE_LOGI("@@[%s::%d]free ptr=%p\n", __FUNCTION__, __LINE__, display);
            free(display);
            display = NULL;
        }
    }
}

static inline void deleteCluts(DVBSubContext *ctx) {
    DVBSubCLUT *clut;
    while (ctx->clutList) {
        clut = ctx->clutList;
        ctx->clutList = clut->next;
        if (clut) {
            free(clut);
        }
    }
}

static void deleteObjects(DVBSubContext *ctx) {
    DVBSubObject *object;
    while (ctx->objectList) {
        object = ctx->objectList;
        ctx->objectList = object->next;
        if (object) {
            free(object);
        }
    }
}

static inline void deleteRegions(DVBSubContext *ctx) {
    DVBSubRegion *region;
    while (ctx->regionList) {
        region = ctx->regionList;
        ctx->regionList = region->next;
        deleteRegionDisplayList(ctx, region);
        if (region) {
            if (region->pbuf) {
                SUBTITLE_LOGI("[%s::%d] region->p=%p, region=%p,  size=%d\n", __FUNCTION__, __LINE__, region->pbuf, region, region->bufSize);
                SUBTITLE_LOGI("@@[%s::%d] free ptr=%p, region->id=%d\n", __FUNCTION__, __LINE__, region->pbuf, region->id);
                free(region->pbuf);
                region->pbuf = NULL;
            }
            SUBTITLE_LOGI("@@[%s::%d] free ptr=%p\n", __FUNCTION__, __LINE__, region);
            free(region);
            region = NULL;
        }
    }
}

static int dvbsub_read_2bit_string(uint8_t *destbuf, int dbufLen, const uint8_t **srcbuf,
         int bufSize, int nonMod, uint8_t *mapTable) {
    GetBitContext gb;
    int bits;
    int runLen;
    int pixelsReads = 0;
    init_get_bits(&gb, *srcbuf, bufSize << 3);
    while (get_bits_count(&gb) < bufSize << 3 && pixelsReads < dbufLen) {
        bits = get_bits(&gb, 2);
        if (bits) {
            if (nonMod != 1 || bits != 1) {
                if (mapTable)
                    *destbuf++ = mapTable[bits];
                else
                    *destbuf++ = bits;
            }
            pixelsReads++;
        } else {
            bits = get_bits1(&gb);
            if (bits == 1) {
                runLen = get_bits(&gb, 3) + 3;
                bits = get_bits(&gb, 2);
                if (nonMod == 1 && bits == 1) {
                    pixelsReads += runLen;
                } else {
                    if (mapTable)
                        bits = mapTable[bits];
                    while (runLen-- > 0 && pixelsReads < dbufLen) {
                        *destbuf++ = bits;
                        pixelsReads++;
                    }
                }
            } else {
                bits = get_bits1(&gb);
                if (bits == 0) {
                    bits = get_bits(&gb, 2);
                    if (bits == 2) {
                        runLen = get_bits(&gb, 4) + 12;
                        bits = get_bits(&gb, 2);
                        if (nonMod == 1 && bits == 1) {
                            pixelsReads += runLen;
                        } else {
                            if (mapTable)
                                bits = mapTable[bits];
                            while (runLen-- > 0 && pixelsReads < dbufLen) {
                                *destbuf++ = bits;
                                pixelsReads++;
                            }
                        }
                    } else if (bits == 3) {
                        runLen = get_bits(&gb, 8) + 29;
                        bits = get_bits(&gb, 2);
                        if (nonMod == 1 && bits == 1) {
                            pixelsReads += runLen;
                       } else {
                            if (mapTable)
                                bits = mapTable[bits];
                            while (runLen-- > 0 && pixelsReads < dbufLen) {
                                *destbuf++ = bits;
                                pixelsReads++;
                            }
                        }
                    } else if (bits == 1) {
                        pixelsReads += 2;
                        if (mapTable) {
                            bits = mapTable[0];
                        } else {
                            bits = 0;
                        }
                        if (pixelsReads <= dbufLen) {
                            *destbuf++ = bits;
                            *destbuf++ = bits;
                        }
                    } else {
                        (*srcbuf) += (get_bits_count(&gb) + 7) >> 3;
                        return pixelsReads;
                    }
                } else {
                    if (mapTable)
                        bits = mapTable[0];
                    else
                        bits = 0;
                    *destbuf++ = bits;
                    pixelsReads++;
                }
            }
        }
    }
    if (get_bits(&gb, 6)) {
        SUBTITLE_LOGI("[%s::%d] DVBSub error: line overflow\n", __FUNCTION__, __LINE__);
    }
    (*srcbuf) += (get_bits_count(&gb) + 7) >> 3;
    return pixelsReads;
}

static inline int dvbsub_read_4bit_string(uint8_t *destbuf, int dbufLen,
                                   const uint8_t **srcbuf, int bufSize,
                                   int nonMod, uint8_t *mapTable)
{
    GetBitContext gb;
    int bits;
    int runLen;
    int pixelsReads = 0;
    init_get_bits(&gb, *srcbuf, bufSize << 3);
    while (get_bits_count(&gb) < bufSize << 3 && pixelsReads < dbufLen) {
        //if (mState == SUB_STOP)
            //return 0;
        bits = get_bits(&gb, 4);
        if (bits) {
            if (nonMod != 1 || bits != 1) {
                if (mapTable)
                    *destbuf++ = mapTable[bits];
                else
                    *destbuf++ = bits;
            }
            pixelsReads++;
        } else {
            bits = get_bits1(&gb);
            if (bits == 0) {
                runLen = get_bits(&gb, 3);
                if (runLen == 0) {
                    (*srcbuf) += (get_bits_count(&gb) + 7) >> 3;
                    return pixelsReads;
                }
                runLen += 2;

                if (mapTable)
                    bits = mapTable[0];
                else
                    bits = 0;

                while (runLen-- > 0 && pixelsReads < dbufLen) {
                    *destbuf++ = bits;
                    pixelsReads++;
                }
            } else {
                bits = get_bits1(&gb);
                if (bits == 0) {
                    runLen = get_bits(&gb, 2) + 4;
                    bits = get_bits(&gb, 4);
                    if (nonMod == 1 && bits == 1) {
                        pixelsReads += runLen;
                    } else {
                        if (mapTable)
                            bits = mapTable[bits];
                        while (runLen-- > 0 && pixelsReads < dbufLen) {
                            *destbuf++ = bits;
                            pixelsReads++;
                        }
                    }
                } else {
                    bits = get_bits(&gb, 2);
                    if (bits == 2) {
                        runLen = get_bits(&gb, 4) + 9;
                        bits = get_bits(&gb, 4);
                        if (nonMod == 1 && bits == 1) {
                            pixelsReads += runLen;
                       } else {
                            if (mapTable)
                                bits = mapTable[bits];

                            while (runLen-- > 0 && pixelsReads < dbufLen) {
                                //if (mState == SUB_STOP)
                                   // return 0;
                                *destbuf++ = bits;
                                pixelsReads++;
                            }
                        }
                    } else if (bits == 3) {
                        runLen = get_bits(&gb, 8) + 25;
                        bits = get_bits(&gb, 4);
                        if (nonMod == 1 && bits == 1) {
                            pixelsReads += runLen;
                        } else {
                            if (mapTable)
                                bits = mapTable[bits];
                            while (runLen-- > 0 && pixelsReads < dbufLen) {
                                *destbuf++ = bits;
                                pixelsReads++;
                            }
                        }
                    } else if (bits == 1) {
                        pixelsReads += 2;
                        if (mapTable)
                            bits = mapTable[0];
                        else
                            bits = 0;
                        if (pixelsReads <= dbufLen) {
                            *destbuf++ = bits;
                            *destbuf++ = bits;
                        }
                    } else {
                        if (mapTable)
                            bits = mapTable[0];
                        else
                            bits = 0;
                        *destbuf++ = bits;
                        pixelsReads++;
                    }
                }
            }
        }
    }
    if (get_bits(&gb, 8))
        SUBTITLE_LOGI("[%s::%d] DVBSub error: line overflow\n", __FUNCTION__,__LINE__);
    (*srcbuf) += (get_bits_count(&gb) + 7) >> 3;
    return pixelsReads;
}

static int dvbsub_read_8bit_string(uint8_t *destbuf, int dbufLen,
                                   const uint8_t **srcbuf, int bufSize,
                                   int nonMod, uint8_t *mapTable)
{
    const uint8_t *sbufEnd = (*srcbuf) + bufSize;
    int bits;
    int runLen;
    int pixelsReads = 0;
    while (*srcbuf < sbufEnd && pixelsReads < dbufLen) {
        bits = *(*srcbuf)++;
        if (bits) {
            if (nonMod != 1 || bits != 1) {
                if (mapTable)
                    *destbuf++ = mapTable[bits];
                else
                    *destbuf++ = bits;
            }
            pixelsReads++;
        } else {
            bits = *(*srcbuf)++;
            runLen = bits & 0x7f;
            if ((bits & 0x80) == 0) {
                if (runLen == 0) {
                    return pixelsReads;
                }
                if (mapTable)
                    bits = mapTable[0];
                else
                    bits = 0;
                while (runLen-- > 0 && pixelsReads < dbufLen) {
                    *destbuf++ = bits;
                    pixelsReads++;
                }
            } else {
                bits = *(*srcbuf)++;
                if (nonMod == 1 && bits == 1)
                    pixelsReads += runLen;
                if (!mapTable) {
                    while (runLen-- > 0 && pixelsReads < dbufLen) {
                        *destbuf++ = bits;
                        pixelsReads++;
                    }
                }
            }
        }
    }
    if (*(*srcbuf)++)
        SUBTITLE_LOGI("[%s::%d] DVBSub error: line overflow\n", __FUNCTION__, __LINE__);
    return pixelsReads;
}

bool static inline isMore32Bit(int64_t pts) {
    if (((pts >> 32) & HIGH_32_BIT_PTS) > 0) {
        return true;
    }
    return false;
}

int DvbParser::parsePixelDataBlock(DVBSubObjectDisplay *display,
        const uint8_t *buf, int bufSize, int top_bottom, int nonMod) {
    DVBSubRegion *region = getRegion(mContext, display->regionId);
    const uint8_t *bufEnd = buf + bufSize;
    uint8_t *pbuf;
    int xPos, yPos;
    int i;
    uint8_t map2to4[] = { 0x0, 0x7, 0x8, 0xf };
    uint8_t map2to8[] = { 0x00, 0x77, 0x88, 0xff };
    uint8_t map4to8[] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                          0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
                        };
    uint8_t *mapTable;
    SUBTITLE_LOGI("[%s::%d] DVB pixel block size %d, %s field:\n", __FUNCTION__, __LINE__, bufSize, top_bottom ? "bottom" : "top");

    if (region == 0)
        return 0;
    pbuf = region->pbuf;
    xPos = display->xPos;
    yPos = display->yPos;
    /*
    if ((yPos & 1) != top_bottom)
        yPos++;
    */
    //calculation y pos here
    if (top_bottom == 1) {
        yPos++;
    }
    while (buf < bufEnd) {
        int bufcaps = region->bufSize - (yPos * region->width + xPos);
        int needReads = bufEnd - buf;
        if (needReads > bufcaps) {
            SUBTITLE_LOGI("error read reginbuffer:%p end:%p, size=%d", pbuf, pbuf + region->bufSize, region->bufSize);
            SUBTITLE_LOGI("error read to:%p, opacity=%d, readsize=%d", pbuf + (yPos * region->width) + xPos, bufcaps, needReads);
            needReads = bufcaps;
        }

       if (xPos > region->width || yPos > region->height || (needReads < 0)) {
            SUBTITLE_LOGE("Invalid object location!\n");
            return -1;
        }

        switch (*buf++) {
            case DVBSUB_DT_2BP_CODE_STRING:
                if (region->depth == 8)
                    mapTable = map2to8;
                else if (region->depth == 4)
                    mapTable = map2to4;
                else
                    mapTable = NULL;
                xPos += dvbsub_read_2bit_string(
                        pbuf + (yPos * region->width) + xPos,
                        region->width - xPos, &buf,
                        needReads/*bufEnd - buf*/, nonMod, mapTable);
                break;
            case DVBSUB_DT_4BP_CODE_STRING:
                if (region->depth < 4) {
                   SUBTITLE_LOGE("4-bit pixel string in %d-bit region!\n", region->depth);
                    return 0;
                }
                if (region->depth == 8)
                    mapTable = map4to8;
                else
                    mapTable = NULL;

                xPos += dvbsub_read_4bit_string(
                        pbuf + (yPos * region->width) + xPos,
                        region->width - xPos, &buf,
                        needReads/*bufEnd - buf*/, nonMod, mapTable);
                break;
            case DVBSUB_DT_8BP_CODE_STRING:
                if (region->depth < 8) {
                    SUBTITLE_LOGE("8-bit pixel string in %d-bit region!\n", region->depth);
                    return 0;
                }
                xPos += dvbsub_read_8bit_string(
                        pbuf + (yPos * region->width) + xPos,
                        region->width - xPos, &buf,
                        needReads/*bufEnd - buf*/, nonMod, NULL);
                break;
            case DVBSUB_DT_24_MAP_TABLE_DATA:
                if ((bufEnd - buf) < 2) {
                    SUBTITLE_LOGE("Malformed source! break");
                    break;
                }
                map2to4[0] = (*buf) >> 4;
                map2to4[1] = (*buf++) & 0xf;
                map2to4[2] = (*buf) >> 4;
                map2to4[3] = (*buf++) & 0xf;
                break;
            case DVBSUB_DT_28_MAP_TABLE_DATA:
                if ((bufEnd - buf) < 4) {
                    SUBTITLE_LOGE("Malformed source! break");
                    break;
                }
                for (i = 0; i < 4; i++)
                    map2to8[i] = *buf++;
                break;
            case DVBSUB_DT_48_MAP_TABLE_DATA:
                if ((bufEnd - buf) < 16) {
                    SUBTITLE_LOGE("Malformed source! break");
                    break;
                }
                for (i = 0; i < 16; i++)
                    map4to8[i] = *buf++;
                break;
            case DVBSUB_DT_END_OF_OBJECT_LINE:
                xPos = display->xPos;
                yPos += 2;
                break;
            default:
                SUBTITLE_LOGI("[%s::%d] Unknown/unsupported pixel block 0x%x\n",
                     __FUNCTION__, __LINE__, *(buf - 1));
                //when account for unknown pixel block, don't discard current segment. SWPL-7481
                break;
        }
    }
    return 0;
}

int DvbParser::init() {
    int r, g, b, a;
    mContext = new DVBSubContext();
    if (!mContext) {
        SUBTITLE_LOGE("[%s::%d] create DVBSubContext error, memory leak?\n", __FUNCTION__, __LINE__);
    }

    mContext->compositionId = -1;
    mContext->ancillaryId = -1;

    gDefaultClut.id = -1;
    gDefaultClut.next = NULL;
    gDefaultClut.clut4[0] = RGBA(0, 0, 0, 0);
    gDefaultClut.clut4[1] = RGBA(255, 255, 255, 255);
    gDefaultClut.clut4[2] = RGBA(0, 0, 0, 255);
    gDefaultClut.clut4[3] = RGBA(127, 127, 127, 255);
    gDefaultClut.clut16[0] = RGBA(0, 0, 0, 0);
    for (int i = 1; i < 16; i++) {
        if (i < 8) {
            r = (i&1) ? 255 : 0;
            g = (i&2) ? 255 : 0;
            b = (i&4) ? 255 : 0;
        } else {
            r = (i&1) ? 127 : 0;
            g = (i&2) ? 127 : 0;
            b = (i&4) ? 127 : 0;
        }
        gDefaultClut.clut16[i] = RGBA(r, g, b, 255);
    }
    gDefaultClut.clut256[0] = RGBA(0, 0, 0, 0);
    for (int i = 1; i < 256; i++) {
        if (i < 8) {
            r = (i&1) ? 255 : 0;
            g = (i&2) ? 255 : 0;
            b = (i&4) ? 255 : 0;
            a = 63;
        } else {
            switch (i&0x88) {
                case 0x00:
                    r = ((i&1) ? 85 : 0) + ((i&0x10) ? 170 : 0);
                    g = ((i&2) ? 85 : 0) + ((i&0x20) ? 170 : 0);
                    b = ((i&4) ? 85 : 0) + ((i&0x40) ? 170 : 0);
                    a = 255;
                    break;
                case 0x08:
                    r = ((i&1) ? 85 : 0) + ((i&0x10) ? 170 : 0);
                    g = ((i&2) ? 85 : 0) + ((i&0x20) ? 170 : 0);
                    b = ((i&4) ? 85 : 0) + ((i&0x40) ? 170 : 0);
                    a = 127;
                    break;
                case 0x80:
                    r = 127 + ((i&1) ? 43 : 0) + ((i&0x10) ? 85 : 0);
                    g = 127 + ((i&2) ? 43 : 0) + ((i&0x20) ? 85 : 0);
                    b = 127 + ((i&4) ? 43 : 0) + ((i&0x40) ? 85 : 0);
                    a = 255;
                    break;
                case 0x88:
                    r = ((i&1) ? 43 : 0) + ((i&0x10) ? 85 : 0);
                    g = ((i&2) ? 43 : 0) + ((i&0x20) ? 85 : 0);
                    b = ((i&4) ? 43 : 0) + ((i&0x40) ? 85 : 0);
                    a = 255;
                    break;
            }
        }
        gDefaultClut.clut256[i] = RGBA(r, g, b, a);
    }
    return 0;
}

void DvbParser::checkDebug() {
    //dump dvb subtitle bitmap
    #ifdef NEED_DUMP_ANDROID
    char value[PROPERTY_VALUE_MAX] = {0};
    memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("vendor.subtitle.dump", value, "false");
    if (!strcmp(value, "true")) {
        mDumpSub = true;
    }
    #endif
}

void DvbParser::notifySubtitleDimension(int width, int height) {
    if (mNotifier != nullptr) {
        if ((mContext->lastSpuOriginDisplayW != (unsigned int)width) || (mContext->lastSpuOriginDisplayH != (unsigned int)height)) {
            SUBTITLE_LOGI("notifySubtitleDimension: last wxh:(%d x %d), now wxh:(%d x %d)",
                mContext->lastSpuOriginDisplayW, mContext->lastSpuOriginDisplayH, width, height);
            mNotifier->onSubtitleDimension(width, height);
            mContext->lastSpuOriginDisplayW = width;
            mContext->lastSpuOriginDisplayH = height;
        }
    }
}

void DvbParser::notifySubtitleErrorInfo(int error) {
    if (mNotifier != nullptr) {
        SUBTITLE_LOGI("notifySubtitleErrorInfo: %d", error);
        mNotifier->onSubtitleAvailable(error);
    }
}

DvbParser::DvbParser(std::shared_ptr<DataSource> source) {
    mContext = nullptr;
    mDataSource = source;
    mParseType = TYPE_SUBTITLE_DVB;

    mPendingAction = -1;
    mTimeoutThread = std::shared_ptr<std::thread>(new std::thread(&DvbParser::callbackProcess, this));
    init();
    checkDebug();
}

DvbParser::~DvbParser() {
    SUBTITLE_LOGI("%s", __func__);
    mState = SUB_STOP;
    stopParser();
    SUBTITLE_LOGI("stopParser end");
    if (mContext != NULL) {
        deleteRegions(mContext);
        deleteObjects(mContext);
        deleteCluts(mContext);

        if (mContext->displayDefinition != nullptr) {
            free(mContext->displayDefinition);
        }

        while (mContext->displayList) {
            DVBSubRegionDisplay *display = mContext->displayList;
            if (display) {
                mContext->displayList = display->next;
                SUBTITLE_LOGI("[%s::%d] free ptr=%p\n", __FUNCTION__, __LINE__, display);
                free(display);
                display = NULL;
            }
        }
        delete mContext;
        mContext = nullptr;
    }

    {   //To TimeoutThread: wakeup! we are exiting...
        std::unique_lock<std::mutex> autolock(mMutex);
        mPendingAction = 1;
        mCv.notify_all();
    }
    mTimeoutThread->join();
}

bool DvbParser::updateParameter(int type, void *data) {
    if (TYPE_SUBTITLE_DVB == type) {
        DvbParam *pDvbParam = (DvbParam* )data;
        mContext->compositionId = pDvbParam->compositionId;
        mContext->ancillaryId = pDvbParam->ancillaryId;
        if (mContext->compositionId <= 0 && mContext->ancillaryId <= 0) {
            //invalid, reset to -1
            mContext->compositionId = -1;
            mContext->ancillaryId = -1;
        }
    }
    return true;
}

int DvbParser::parseObjectSegment(const uint8_t *buf, int bufSize) {
    const uint8_t *bufEnd = buf + bufSize;
    const uint8_t *block;
    int objectId;
    DVBSubObject *object;
    DVBSubObjectDisplay *display;
    int topFieldLen, bottomFieldLen;
    int codingMethod, nonModifyingColor;

    objectId = AV_RB16(buf);
    buf += 2;
    object = getObject(mContext, objectId);
    if (!object) {
        SUBTITLE_LOGE("Cannot get object!");
        return 0;
    }
    codingMethod = ((*buf) >> 2) & 3;
    nonModifyingColor = ((*buf++) >> 1) & 1;
    if (codingMethod == 0) {
        topFieldLen = AV_RB16(buf);
        buf += 2;
        bottomFieldLen = AV_RB16(buf);
        buf += 2;
        if (buf + topFieldLen + bottomFieldLen > bufEnd) {
            notifySubtitleErrorInfo(ERROR_DECODER_INVALIDDATA);
            SUBTITLE_LOGE("Field data size too large\n");
            return 0;
        }
        for (display = object->displayList; display;
                display = display->objectListNext) {
            int ret = 0;
            block = buf;
            /*
            ret = parsePixelDataBlock(display, block, topFieldLen, (totalObject >=SINGLE_OBJECT_DATA && (cntObject % SINGLE_OBJECT_DATA != SINGLE_FIRST_OBJECT_DATA)) ? BOTTOM_FIELD : TOP_FIELD , nonModifyingColor);
            */
            //calculation y pos at last
            ret = parsePixelDataBlock(display, block, topFieldLen, TOP_FIELD , nonModifyingColor);
            if (ret == -1)
                return ret;

            if (bottomFieldLen > 0) {
                block = buf + topFieldLen;
            } else {
                bottomFieldLen = topFieldLen;
            }
            /*
            ret = parsePixelDataBlock(display, block, bottomFieldLen,  (totalObject >=SINGLE_OBJECT_DATA && (cntObject % SINGLE_OBJECT_DATA != SINGLE_FIRST_OBJECT_DATA)) ? TOP_FIELD : BOTTOM_FIELD, nonModifyingColor);
            */
            //calculation y pos at last
            ret = parsePixelDataBlock(display, block, bottomFieldLen,  BOTTOM_FIELD, nonModifyingColor);
            if (ret == -1)
                return ret;
        }
    } else {
        notifySubtitleErrorInfo(ERROR_DECODER_INVALIDDATA);
        SUBTITLE_LOGE("Unknown object coding %d\n", codingMethod);
        return -1;
    }
    return 0;
}

void DvbParser::parseClutSegment(const uint8_t *buf, int bufSize) {
    const uint8_t *bufEnd = buf + bufSize;
    int clutId;
    DVBSubCLUT *clut;
    int entry_id, depth, fullRange;
    int y, cr, cb, alpha;
    int r, g, b, r_add, g_add, b_add;
    SUBTITLE_LOGE("DVB clut packet:\n");

    clutId = *buf++;
    buf += 1;
    clut = getClut(mContext, clutId);

    if (!clut) {
        clut = (DVBSubCLUT *) malloc(sizeof(DVBSubCLUT));
        if (!clut) {
            SUBTITLE_LOGE("[%s::%d]malloc error! \n", __FUNCTION__, __LINE__);
            return;
        }
        SUBTITLE_LOGI("@@[%s::%d]malloc ptr=%p, size=%d\n", __FUNCTION__, __LINE__, clut, sizeof(DVBSubCLUT));

        memcpy(clut, &gDefaultClut, sizeof(DVBSubCLUT));
        clut->id = clutId;
        clut->next = mContext->clutList;
        mContext->clutList = clut;
    }
    while (buf + 4 < bufEnd) {
        entry_id = *buf++;
        depth = (*buf) & 0xe0;
        if (depth == 0) {
            notifySubtitleErrorInfo(ERROR_DECODER_INVALIDDATA);
            SUBTITLE_LOGE("Invalid clut depth 0x%x!\n", *buf);
            return;
        }
        fullRange = (*buf++) & 1;
        if (fullRange) {
            y = *buf++;
            cr = *buf++;
            cb = *buf++;
            alpha = *buf++;
        } else {
            y = buf[0] & 0xfc;
            cr = (((buf[0] & 3) << 2) | ((buf[1] >> 6) & 3)) << 4;
            cb = (buf[1] << 2) & 0xf0;
            alpha = (buf[1] << 6) & 0xc0;
            buf += 2;
        }
        if (y == 0) {
            cr = 0;
            cb = 0;
            alpha = 0xff;
        }
        YUV_TO_RGB1_CCIR(cb, cr);
        YUV_TO_RGB2_CCIR(r, g, b, y);
        SUBTITLE_LOGE("clut %d := (%d,%d,%d,%d)\n", entry_id, r, g, b, alpha);
        if (depth & 0x80)
            clut->clut4[entry_id] = RGBA(r, g, b, 255-alpha);
        if (depth & 0x40)
            clut->clut16[entry_id] = RGBA(r, g, b, 255-alpha);
        if (depth & 0x20)
            clut->clut256[entry_id] = RGBA(r, g, b, 255-alpha);
    }
}

void DvbParser::parseRegionSegment(const uint8_t *buf, int bufSize) {
    const uint8_t *bufEnd = buf + bufSize;
    int regionId, objectId;
    DVBSubRegion *region;
    DVBSubObject *object;
    DVBSubObjectDisplay *display;
    int fill;
    if (bufSize < 10) {
        return;
    }

    regionId = *buf++;
    region = getRegion(mContext, regionId);
    if (!region) {
        region = (DVBSubRegion *)malloc(sizeof(DVBSubRegion));
        if (!region) {
            SUBTITLE_LOGE("[%s::%d]malloc error! \n", __FUNCTION__, __LINE__);
            return;
        }
        SUBTITLE_LOGI("@@[%s::%d]malloc ptr=%p, size=%d\n", __FUNCTION__, __LINE__, region, sizeof(DVBSubRegion));
        memset(region, 0, sizeof(DVBSubRegion));
        region->id = regionId;
        region->next = mContext->regionList;
        mContext->regionList = region;
    }
    fill = ((*buf++) >> 3) & 1;
    region->width = AV_RB16(buf);
    buf += 2;
    region->height = AV_RB16(buf);
    buf += 2;
    if (region->width * region->height != region->bufSize) {
        if (region->pbuf) {
            SUBTITLE_LOGI("free region->buf=%p\n", region->pbuf);
            SUBTITLE_LOGI("@@[%s::%d] free ptr=%p\n", __FUNCTION__,__LINE__, region->pbuf);
            free(region->pbuf);
            region->pbuf = NULL;
        }
        region->bufSize = region->width * region->height;
        region->pbuf = (uint8_t *) malloc(region->bufSize);
        if (!region->pbuf) {
            SUBTITLE_LOGE("[%s::%d]malloc error! \n", __FUNCTION__, __LINE__);
            return;
        }
        SUBTITLE_LOGI("@@[%s::%d] malloc ptr=%p, size=%d\n",__FUNCTION__, __LINE__, region->pbuf, region->bufSize);
        SUBTITLE_LOGI("malloc region->buf=%p, size=%d\n", region->pbuf, region->bufSize);
        fill = 1;
    }
    region->depth = 1 << (((*buf++) >> 2) & 7);
    if (region->depth < 2 || region->depth > 8) {
        SUBTITLE_LOGI("region depth %d is invalid\n", region->depth);
        region->depth = 4;
    }
    region->clut = *buf++;

    if (region->depth == 8) {
        region->bgcolor = *buf++;
        buf += 1;
    } else {
        buf += 1;
        if (region->depth == 4) {
            region->bgcolor = (((*buf++) >> 4) & 15);
        } else {
            region->bgcolor = (((*buf++) >> 2) & 3);
        }
    }
    SUBTITLE_LOGI("Region %d, (%dx%d)\n", regionId, region->width, region->height);
    if (fill) {
        memset(region->pbuf, region->bgcolor, region->bufSize);
        SUBTITLE_LOGI("Fill region (%d)\n", region->bgcolor);
    }
    deleteRegionDisplayList(mContext, region);
    while (buf + 5 < bufEnd) {
        objectId = AV_RB16(buf);
        buf += 2;
        object = getObject(mContext, objectId);
        if (!object) {
            object = (DVBSubObject *)calloc(1, sizeof(DVBSubObject));
            if (!object) {
                SUBTITLE_LOGE("[%s::%d]DVBSubObject calloc error! \n", __FUNCTION__, __LINE__);
                return;
            }
            SUBTITLE_LOGI("@@[%s::%d]malloc ptr=%p, size=%d\n",
                    __FUNCTION__, __LINE__, object, sizeof(DVBSubObject));
            object->id = objectId;
            object->next = mContext->objectList;
            mContext->objectList = object;
        }
        object->type = (*buf) >> 6;
        display = (DVBSubObjectDisplay *)calloc(1, sizeof(DVBSubObjectDisplay));
        if (!display) {
            SUBTITLE_LOGE("[%s::%d]DVBSubObjectDisplay calloc error! \n", __FUNCTION__, __LINE__);
            return;
        }
        SUBTITLE_LOGI("@@[%s::%d]malloc ptr=%p, size=%d\n", __FUNCTION__, __LINE__, display, sizeof(DVBSubObjectDisplay));

        display->objectId = objectId;
        display->regionId = regionId;
        display->xPos = AV_RB16(buf) & 0xfff;
        buf += 2;
        display->yPos = AV_RB16(buf) & 0xfff;
        buf += 2;
        if ((object->type == DVBSUB_OT_BASIC_CHARACTER || object->type == DVBSUB_OT_COMPOSITE_STRING)
                && buf + 1 < bufEnd) {
            display->fgcolor = *buf++;
            display->bgcolor = *buf++;
        }
        display->regionListNext = region->displayList;
        region->displayList = display;
        display->objectListNext = object->displayList;
        object->displayList = display;
    }
    return;
}

void DvbParser::parsePageSegment(const uint8_t *buf, int bufSize) {
    DVBSubRegionDisplay *display;
    DVBSubRegionDisplay *tmpDisplayList, **tmpPtr;
    const uint8_t *bufEnd = buf + bufSize;
    int regionId;
    int page_state;
    if (bufSize < 1) {
        return;
    }

    mContext->timeOut = *buf++;
    if (mContext->timeOut >= DVB_TIME_OUT_LONG_DURATION) {
        mContext->timeOut = DVB_TIME_OUT_ADJUST;
    }
    page_state = ((*buf++) >> 2) & 3;
    SUBTITLE_LOGI("Page time out %ds, state %d\n", mContext->timeOut, page_state);
    if (page_state == DVBSUB_PCS_STATE_ACQUISITION || page_state == DVBSUB_PCS_STATE_MODE_CHANGE) {
        SUBTITLE_LOGI("[%s::%d] dvbsub_parse_page_segment\n", __FUNCTION__,__LINE__);
        deleteRegions(mContext);
        deleteObjects(mContext);
        deleteCluts(mContext);
    }
    tmpDisplayList = mContext->displayList;
    mContext->displayList = NULL;
    mContext->displayListSize = 0;
    while (buf + 5 < bufEnd) {
        regionId = *buf++;
        buf += 1;
        display = tmpDisplayList;
        tmpPtr = &tmpDisplayList;
        while (display && display->regionId != regionId) {
            tmpPtr = &display->next;
            display = display->next;
        }
        if (!display) {
            display = (DVBSubRegionDisplay *)calloc(1, sizeof(DVBSubRegionDisplay));
            if (!display) {
                SUBTITLE_LOGE("DVB SubRegion Display calloc fail!\n");
                return;
            }
        }
        SUBTITLE_LOGI("@@[%s::%d]malloc ptr=%p, size=%d\n", __FUNCTION__, __LINE__, display, sizeof(DVBSubObjectDisplay));

        display->regionId = regionId;
        display->xPos = AV_RB16(buf);
        buf += 2;
        display->yPos = AV_RB16(buf);
        buf += 2;
        *tmpPtr = display->next;
        display->next = mContext->displayList;
        mContext->displayList = display;
        mContext->displayListSize++;
        SUBTITLE_LOGI("Region %d, (%d,%d)\n", regionId, display->xPos,display->yPos);
    }
    while (tmpDisplayList) {
        display = tmpDisplayList;
        tmpDisplayList = display->next;
        if (display) {
            SUBTITLE_LOGI("@@[%s::%d] free ptr=%p\n", __FUNCTION__,__LINE__, display);
            free(display);
            display = NULL;
        }
    }
}

void DvbParser::saveResult2Spu(std::shared_ptr<AML_SPUVAR> spu) {
    DVBSubRegion *region;
    DVBSubRegionDisplay *display;
    DVBSubCLUT *clut;
    uint32_t *clut_table;
    int xPos, yPos, width, height;
    int x, y, yOff, xOff;
    uint32_t *pbuf = NULL;
    char filename[32];
    static int filenoIndex = 0;
    DVBSubDisplayDefinition *displayDef = mContext->displayDefinition;
    xPos = -1;
    yPos = -1;
    width = 0;
    height = 0;
    for (display = mContext->displayList; display; display = display->next) {
        region = getRegion(mContext, display->regionId);
        if (!region)
            return;
        if (xPos == -1) {
            xPos = display->xPos;
            yPos = display->yPos;
            width = region->width;
            height = region->height;
        } else {
            if (display->xPos < xPos) {
                width += (xPos - display->xPos);
                xPos = display->xPos;
            }

            if (display->yPos < yPos) {
                // avoid different region bitmap cross;20150906 bug111420
                if (yPos - display->yPos < region->height) {
                    height += region->height;
                    yPos = yPos - region->height;
                    display->yPos = yPos;
                } else {
                    height += (yPos - display->yPos);
                    yPos = display->yPos;
                }
            }

            if (display->xPos + region->width > xPos + width) {
                width = display->xPos + region->width - xPos;
            }

            if (display->yPos + region->height > yPos + height) {
                height = display->yPos + region->height - yPos;
            }
        }
        SUBTITLE_LOGI("@@[%s::%d] xPos=%d, yPos=%d, width=%d, height=%d, region->id=%d\n", __FUNCTION__, __LINE__, xPos, yPos, width, height, region->id);
    }

    if (xPos >= 0) {
        spu->buffer_size = width * height * 4;
        pbuf = (uint32_t *)malloc(width * height * 4);
        SUBTITLE_LOGI("@@[%s::%d]malloc ptr=%p, size=%d\n", __FUNCTION__, __LINE__, pbuf, width * height * 4);
        SUBTITLE_LOGI("malloc pbuf=%p, size=%d \n", region->pbuf, width * height * 4);
        if (!pbuf) {
            SUBTITLE_LOGE("save_display_set malloc fail, width=%d, height=%d \n", width, height);
            free(pbuf);
            return;
        }
        memset(pbuf, 0, width * height * 4);
        if (spu->spu_data) free(spu->spu_data);
        spu->spu_data = (unsigned char *)malloc(spu->buffer_size);
        if (!spu->spu_data) {
            SUBTITLE_LOGE("[%s::%d] malloc error!\n", __FUNCTION__, __LINE__);
            free(pbuf);
            return ;
        }
        memset(spu->spu_data, 0, spu->buffer_size);
        spu->spu_width = width;
        spu->spu_height = height;
        spu->spu_start_x = xPos;
        spu->spu_start_y = yPos;

        if (displayDef && displayDef->width >=0 && displayDef->height >=0) {
            spu->spu_origin_display_w = displayDef->width;
            spu->spu_origin_display_h = displayDef->height;
        }

        for (display = mContext->displayList; display; display = display->next) {
            unsigned check_err = 0;
            region = getRegion(mContext, display->regionId);
            if (!region) {
                free(pbuf);
                return;
            }
            xOff = display->xPos - xPos;
            yOff = display->yPos - yPos;
            clut = getClut(mContext, region->clut);
            if (clut == 0) {
                free(pbuf);
                return;//clut = &gDefaultClut;
            }
            switch (region->depth) {
                case 2:
                    clut_table = clut->clut4;
                    break;
                case 8:
                    clut_table = clut->clut256;
                    break;
                case 4:
                default:
                    clut_table = clut->clut16;
                    break;
            }

            SUBTITLE_LOGI("@@[%s::%d]xOff=%d, yOff=%d, width=%d, height=%d, region->width=%d, region->height=%d\n",
                    __FUNCTION__, __LINE__, xOff, yOff, width, height, region->width, region->height);

            check_err = (region->height-1+yOff)*width*4 + (xOff+region->width-1)*4 + 3;
            if ((check_err >= DVB_SUB_SIZE) || (region->height > 1080) || (region->width > 1920)) {
                SUBTITLE_LOGI("@@[%s::%d] sub data err!! check_err=%d\n", __FUNCTION__, __LINE__,check_err);
                free(pbuf);
                return;
            }

            for (y = 0; y < region->height; y++) {
                for (x = 0; x < region->width; x++) {
                    pbuf[((y + yOff) * width) + xOff + x] =
                        clut_table[region->pbuf[y * region->width + x]];
                    spu->spu_data[((y + yOff) * width * 4) + (xOff + x) * 4] =
                        pbuf[((y + yOff) * width) + xOff + x] & 0xff;
                    spu->spu_data[((y + yOff) * width * 4) + (xOff + x) * 4 + 1] =
                        (pbuf[((y + yOff) * width) + xOff + x] >> 8) & 0xff;
                    spu->spu_data[((y + yOff) * width * 4) + (xOff + x) * 4 + 2] =
                        (pbuf[((y + yOff) * width) + xOff + x] >> 16) & 0xff;
                    spu->spu_data[((y + yOff) * width * 4) + (xOff + x) * 4 + 3] =
                        (pbuf[((y + yOff) * width) + xOff + x] >> 24) & 0xff;
                }
            }
        }

       if (mDumpSub) {
            #ifdef NEED_DUMP_ANDROID
            snprintf(filename, sizeof(filename), "./data/subtitleDump/dvb(%lld)", spu->pts);
            #endif
            #ifdef NEED_DUMP_LINUX
            snprintf(filename, sizeof(filename), "/tmp/dvb(%lld)", spu->pts);
            #endif
            save2BitmapFile(filename, pbuf, width, height);
       }

        //set_subtitle_height(height);
        SUBTITLE_LOGI("## save_display_set: %d, (%d,%d)--(%d,%d),(%d,%d)---\n",
             filenoIndex, width, height, spu->spu_width,
             spu->spu_height, spu->spu_start_x, spu->spu_start_y);
        if (pbuf) {
            SUBTITLE_LOGI("free pbuf=%p\n", region->pbuf);
            SUBTITLE_LOGI("@@[%s::%d]free ptr=%p\n", __FUNCTION__, __LINE__, pbuf);
            free(pbuf);
        }
    }
    filenoIndex++;
}

void DvbParser::parseDisplayDefinitionSegment(const uint8_t *buf, int bufSize) {
    DVBSubDisplayDefinition *displayDef = mContext->displayDefinition;
    int ddsVersion, infoByte;

    if (bufSize < 5) return;

    infoByte = bytestream_get_byte(&buf);
    ddsVersion = infoByte >> 4;
    if (displayDef != nullptr && displayDef->version == ddsVersion)
        return;     // already have this display definition version

    if (displayDef == nullptr) {
        displayDef = (DVBSubDisplayDefinition *)malloc(sizeof(DVBSubDisplayDefinition));
        SUBTITLE_LOGI("[%s::%d]malloc ptr=%p, size=%d\n",__FUNCTION__, __LINE__, displayDef, sizeof(DVBSubDisplayDefinition));
        mContext->displayDefinition = displayDef;
    }

    if (displayDef == nullptr) {
        SUBTITLE_LOGE("[%s::%d]malloc error! \n", __FUNCTION__, __LINE__);
        return;
    }

    displayDef->version = ddsVersion;
    displayDef->x = 0;
    displayDef->y = 0;
    displayDef->width = bytestream_get_be16(&buf) + 1;
    displayDef->height = bytestream_get_be16(&buf) + 1;

    // display_window_flag
    if (infoByte & 1 << 3) {
        if (bufSize < 13) return;

        int horizontalMin = bytestream_get_be16(&buf);
        int horizontalMax = bytestream_get_be16(&buf);
        int verticalMin = bytestream_get_be16(&buf);
        int verticalMax = bytestream_get_be16(&buf);
        displayDef->x = horizontalMin;
        displayDef->y = verticalMin;
        displayDef->width = horizontalMax - horizontalMin + 1;
        displayDef->height = verticalMax - verticalMin +1;
    }
    SUBTITLE_LOGI("-[%s],displayDef->width=%d,height=%d,infoByte=%d,bufSize=%d--\n",__FUNCTION__,
        displayDef->width, displayDef->height, infoByte, bufSize);
    return;
}

int DvbParser::displayEndSegment(std::shared_ptr<AML_SPUVAR> spu) {
    // Dvb multiply is 90
    spu->m_delay = spu->pts + mContext->timeOut*1000*90;
    saveResult2Spu(spu);
    return 1;
}

int DvbParser::decodeSubtitle(std::shared_ptr<AML_SPUVAR> spu, char *pSrc, const int size) {
    const uint8_t *buf = (uint8_t *)pSrc;
    int bufSize = size;
    const uint8_t *p, *pEnd;
    int segmentType;
    int pageId;
    int segmentLength;
    int dataSize;
    //for special subtitle who has 4 or more object segments
    //int cnt_object = 0;
    int totalObject = 0;
    int total_RegionSegment = 0;
    spu->spu_width = 0;
    spu->spu_height = 0;
    spu->spu_start_x = 0;
    spu->spu_start_y = 0;
    //default spu display in windows width and height
    spu->spu_origin_display_w = 720;
    spu->spu_origin_display_h = 576;

    if (bufSize <= 6 || *buf != 0x0f) {
        SUBTITLE_LOGI("incomplete or broken packet");
        return -1;
    }

    p = buf;
    pEnd = buf + bufSize;
    while (pEnd - p >= 6 && *p == 0x0f) {
        if (mState == SUB_STOP) {
            return -1;
        }

        p += 1;
        segmentType = *p++;
        pageId = AV_RB16(p);
        p += 2;
        segmentLength = AV_RB16(p);
        p += 2;
        if (pEnd - p < segmentLength) {
            notifySubtitleErrorInfo(ERROR_DECODER_LOSEDATA);
            SUBTITLE_LOGI("incomplete or broken packet");
            return -1;
        }
        SUBTITLE_LOGI("## dvbsub_decode segmentType=%x, pageId=%d, cpage=%d, apage=%d",
            segmentType, pageId, mContext->compositionId, mContext->ancillaryId);
        if (pageId == mContext->compositionId
                || pageId == mContext->ancillaryId
                || mContext->compositionId == -1
                || mContext->ancillaryId == -1) {
            int ret = 0;
            switch (segmentType) {
                case DVBSUB_REGION_SEGMENT:
                    total_RegionSegment ++;
                    break;
                case DVBSUB_CLUT_SEGMENT:
                    break;
                case DVBSUB_OBJECT_SEGMENT:
                    totalObject++;
                    break;
                default:
                    SUBTITLE_LOGI("Subtitling segment type 0x%x, page id %d, length %d\n", segmentType, pageId, segmentLength);
                    break;
            }
        }
        p += segmentLength;
    }
    SUBTITLE_LOGI("dvbsub_decode, object segment total:%d, region segment total:%d", totalObject, total_RegionSegment);
    p = buf;
    pEnd = buf + bufSize;

    while (pEnd - p >= 6 && *p == 0x0f) {
        if (mState == SUB_STOP) {
            return -1;
        }

        p += 1;
        segmentType = *p++;
        pageId = AV_RB16(p);
        p += 2;
        segmentLength = AV_RB16(p);
        p += 2;
        if (pEnd - p < segmentLength) {
            SUBTITLE_LOGI("incomplete or broken packet");
            return -1;
        }
        SUBTITLE_LOGI("## dvbsub_decode segmentType=%x", segmentType);
        if (pageId == mContext->compositionId
                || pageId == mContext->ancillaryId
                || mContext->compositionId == -1
                || mContext->ancillaryId == -1) {
            int ret = 0;
            switch (segmentType) {
                case DVBSUB_PAGE_SEGMENT:
                    parsePageSegment(p, segmentLength);
                    dataSize = displayEndSegment(spu);
                    break;
                case DVBSUB_REGION_SEGMENT:
                    parseRegionSegment(p, segmentLength);
                    break;
                case DVBSUB_CLUT_SEGMENT:
                    parseClutSegment(p, segmentLength);
                    break;
                case DVBSUB_OBJECT_SEGMENT:
                    /*
                    cnt_object++;
                    if (totalObject >= SINGLE_OBJECT_DATA && total_RegionSegment != REGION_SEGMENT_CNT) {//only ==2, change top and bottom field
                        cnt_object = SINGLE_FIRST_OBJECT_DATA;
                    }
                    */
                    //calculation y pos at last
                    ret = parseObjectSegment(p, segmentLength);
                    if (ret == -1) return -1;
                    break;
                case DVBSUB_DISPLAYDEFINITION_SEGMENT:
                    parseDisplayDefinitionSegment(p, segmentLength);
                    break;
                case DVBSUB_DISPLAY_SEGMENT:
                    dataSize = displayEndSegment(spu);
                    break;
                case DVBSUB_ST_STUFFING:
                    dataSize = displayEndSegment(spu);
                    break;
                default:
                    SUBTITLE_LOGI("Subtitling segment type 0x%x, page id %d, length %d\n", segmentType, pageId, segmentLength);
                    break;
            }
        }
        p += segmentLength;
    }

    return p - buf;
}


int DvbParser::getDvbSpu() {
    char tmpbuf[8] = {0};
    int64_t packetHeader = 0;

    while (mDataSource->read(tmpbuf, 1) == 1) {
        if (mState == SUB_STOP) {
            return 0;
        }


        packetHeader = ((packetHeader<<8) & 0x000000ffffffff00) | tmpbuf[0];
        if ((packetHeader & 0xffffffff) == 0x000001bd) {
            SUBTITLE_LOGI("## [Hardware Demux]  get_dvb_teletext_spu hardware demux dvb %x,%llx,-----------\n",
                    tmpbuf[0], packetHeader & 0xffffffffff);
            return hwDemuxParse();
        } else if (((packetHeader & 0xffffffffff)>>8) == AML_PARSER_SYNC_WORD
                && (((packetHeader & 0xff)== 0x77) || ((packetHeader & 0xff)==0xaa))) {
            SUBTITLE_LOGI("## 222  get_dvb_teletext_spu soft demux dvb %x,%llx,-----------\n",
                    tmpbuf[0], packetHeader & 0xffffffffff);
            return softDemuxParse();
        }
        //TODO for coverity, useless_continue
        /*else {
            // advance header, not report error if no problem.
            if (tmpbuf[0] == 0xFF) {
                if (packetHeader == 0xFF || packetHeader == 0xFFFF || packetHeader == 0xFFFFFF
                    || packetHeader == 0xFFFFFFFF || packetHeader == 0xFFFFFFFFFF) {
                    continue;
                }
            } else if (tmpbuf[0] == 0) {
                if (packetHeader == 0xff00 || packetHeader == 0xff0000 || packetHeader == 0xffffffff00 || packetHeader == 0xffffff0000) {
                    continue;
                }
            } else if (tmpbuf[0] == 1 && (packetHeader == 0xffff000001 || packetHeader == 0xff000001)) {
                continue;
            }

            //SUBTITLE_LOGE("dvb package header error: %x, %llx",tmpbuf[0], packetHeader);
        }*/
    }

    return 0;
}


int DvbParser::hwDemuxParse() {
    char tmpbuf[256] = {0};
    int64_t pts = 0, dts = 0;
    int64_t tempPts = 0, tempDts = 0;
    int packetLen = 0, pesHeaderLen = 0, realityPacketLen = 0;
    bool needSkipData = false;
    int ret = 0;
    int avil = 0;

    if (mDataSource->read(tmpbuf, 2) == 2) {
        packetLen = (tmpbuf[0] << 8) | tmpbuf[1];
        if (packetLen >= 3) {
            if (mDataSource->read(tmpbuf, 3) == 3) {
                packetLen -= 3;
                pesHeaderLen = tmpbuf[2];
                if (packetLen >= pesHeaderLen) {
                    if ((tmpbuf[1] & 0xc0) == 0x80) {
                        if (mDataSource->read(tmpbuf, pesHeaderLen) == pesHeaderLen) {
                            tempPts = (int64_t)(tmpbuf[0] & 0xe) << 29;
                            tempPts = tempPts | ((tmpbuf[1] & 0xff) << 22);
                            tempPts = tempPts | ((tmpbuf[2] & 0xfe) << 14);
                            tempPts = tempPts | ((tmpbuf[3] & 0xff) << 7);
                            tempPts = tempPts | ((tmpbuf[4] & 0xfe) >> 1);
                            pts = tempPts; // - pts_aligned;
                            packetLen -= pesHeaderLen;
                        }
                    } else if ((tmpbuf[1] & 0xc0) == 0xc0) {
                        if (mDataSource->read(tmpbuf, pesHeaderLen)  == pesHeaderLen) {
                            tempPts = (int64_t)(tmpbuf[0] & 0xe) << 29;
                            tempPts = tempPts | ((tmpbuf[1] & 0xff) << 22);
                            tempPts = tempPts | ((tmpbuf[2] & 0xfe) << 14);
                            tempPts = tempPts | ((tmpbuf[3] & 0xff) << 7);
                            tempPts = tempPts | ((tmpbuf[4] & 0xfe) >> 1);
                            pts = tempPts; // - pts_aligned;
                            tempDts = (int64_t)(tmpbuf[5] & 0xe) << 29;
                            tempDts = tempDts | ((tmpbuf[6] & 0xff) << 22);
                            tempDts = tempDts | ((tmpbuf[7] & 0xfe) << 14);
                            tempDts = tempDts | ((tmpbuf[8] & 0xff) << 7);
                            tempDts = tempDts | ((tmpbuf[9] & 0xfe) >> 1);
                            dts = tempDts; // - pts_aligned;
                            packetLen -= pesHeaderLen;
                        }
                    } else {
                        needSkipData = true;
                    }
                } else {
                    needSkipData = true;
                }
            }
        } else {
            needSkipData = true;
        }

        if (needSkipData) {
            if (packetLen < 0 || packetLen > INT_MAX) {
                SUBTITLE_LOGE("illegal packetLen!!!\n");
                return false;
            }
            for (int iii = 0; iii < packetLen; iii++) {
                char tmp;
                if (mDataSource->read(&tmp, 1) == 0) {
                    break;
                }
            }
        } else if ((pts) && (packetLen > 0)) {
            char *buf = NULL;
            if ((packetLen) > (OSD_HALF_SIZE * 4)) {
                SUBTITLE_LOGE("dvb packet is too big\n\n");
                return -1;
            }

            std::shared_ptr<AML_SPUVAR> spu = std::make_shared<AML_SPUVAR>();
            spu->sync_bytes = AML_PARSER_SYNC_WORD;
            spu->subtitle_type = TYPE_SUBTITLE_DVB;
            spu->pts = pts;
            if (isMore32Bit(spu->pts) && !isMore32Bit(mDataSource->getSyncTime())) {
                SUBTITLE_LOGI("SUB PTS is greater than 32 bits, before subpts: %lld, vpts:%lld", spu->pts, mDataSource->getSyncTime());
                spu->pts &= TSYNC_32_BIT_PTS;
            }

            SUBTITLE_LOGE("[%s::%d]synctime:%lld subpts:%lld\n", __FUNCTION__, __LINE__, mDataSource->getSyncTime(),spu->pts);
            //If gap time is large than 9 secs, think pts skip,notify info
            if (abs(mDataSource->getSyncTime() - spu->pts) > 9*90000) {
                SUBTITLE_LOGE("[%s::%d]pts skip, notify time error!\n", __FUNCTION__, __LINE__);
                notifySubtitleErrorInfo(ERROR_DECODER_TIMEERROR);
            }
            mDataSource->read(tmpbuf, 2) ;
            packetLen -= 2;
            //tempPts = ((tmpbuf[0] << 8) | tmpbuf[1]); //for coverity
            buf = (char *)malloc(packetLen);
            if (buf) {
                SUBTITLE_LOGI("packetLen is %d, pts is %llx, delay is %llx,\n", packetLen, spu->pts, tempPts);
            } else {
                SUBTITLE_LOGI("packetLen buf malloc fail!!!,\n");
            }

            if (buf) {
                memset(buf, 0x0, packetLen);
                realityPacketLen = mDataSource->read(buf, packetLen);
                if (realityPacketLen == packetLen) {
                    SUBTITLE_LOGI("start decode dvb subtitle\n\n");
                    ret = decodeSubtitle(spu, buf, packetLen);
                    SUBTITLE_LOGI("dump-pts-hwdmx parse ret:%d,width:%d, height:%d, buffer_size:%d spu->pts:%lld", ret, spu->spu_width, spu->spu_height, spu->buffer_size, spu->pts);
                    if (ret != -1) {
                        SUBTITLE_LOGI("dump-pts-hwdmx!success pts(%lld)frame was add\n", spu->pts);
                        addDecodedItem(std::shared_ptr<AML_SPUVAR>(spu));
                        notifySubtitleDimension(spu->spu_origin_display_w, spu->spu_origin_display_h);
                        {   // for signal notification.
                            std::unique_lock<std::mutex> autolock(mMutex);
                            mPendingAction = 1;
                            mCv.notify_all();
                        }
                    } else {
                        SUBTITLE_LOGI("dump-pts-hwdmx!error pts(%lld) frame was abandon\n", spu->pts);
                        free(buf);

                        return -1;
                    }
                } else if (realityPacketLen != packetLen){
                        SUBTITLE_LOGI("realityPacketLen Error packetLen:%d reality packetLen:%d",packetLen, realityPacketLen);
                        free(buf);
                        return 1;
                }

                SUBTITLE_LOGI("packetLen buf free=%p\n", buf);
                SUBTITLE_LOGI("[%s::%d] free ptr=%p\n", __FUNCTION__, __LINE__, buf);
                free(buf);
            }

        }
    }else {
        notifySubtitleErrorInfo(ERROR_DECODER_TIMEERROR);
    }

    return ret;
}

int DvbParser::softDemuxParse() {
    char tmpbuf[256] = {0};
    int64_t pts = 0, ptsDiff = 0;
    int ret = 0;
    int dataLen = 0;
    char *data = NULL;
    int avil = 0;

    SUBTITLE_LOGI("## 333 get_dvb_spu %x,%x,%x,%x-------------\n",tmpbuf[0], tmpbuf[1], tmpbuf[2], tmpbuf[3]);
    if (mDataSource->read(tmpbuf, 19) == 19) {
        dataLen = subPeekAsInt32(tmpbuf + 3);
        pts     = subPeekAsInt64(tmpbuf + 7);
        ptsDiff = subPeekAsInt32(tmpbuf + 15);


        std::shared_ptr<AML_SPUVAR> spu(new AML_SPUVAR());
        spu->sync_bytes = AML_PARSER_SYNC_WORD;
        spu->subtitle_type = TYPE_SUBTITLE_DVB;
        SUBTITLE_LOGI("@@[%s::%d]malloc ptr=%p, size=%d\n",__FUNCTION__, __LINE__, spu->spu_data, DVB_SUB_SIZE);
        spu->pts = pts;
        SUBTITLE_LOGI("fmq send pts:%lld\n", spu->pts);
        SUBTITLE_LOGI("## 4444 datalen=%d, pts=%llx,delay=%llx, diff=%llx, data: %x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x-------------\n",
                dataLen, pts, spu->m_delay, ptsDiff,
                tmpbuf[0], tmpbuf[1], tmpbuf[2], tmpbuf[3], tmpbuf[4],
                tmpbuf[5], tmpbuf[6], tmpbuf[7], tmpbuf[8], tmpbuf[9],
                tmpbuf[10], tmpbuf[11], tmpbuf[12], tmpbuf[13], tmpbuf[14]);

        data = (char *)malloc(dataLen);
        if (!data) {
            SUBTITLE_LOGI("data buf malloc fail!\n");
            return -1;
        }
        memset(data, 0x0, dataLen);
        ret = mDataSource->read(data, dataLen);
        if (ret <= 0) {
            SUBTITLE_LOGI("no data now\n");
        }
        ret = decodeSubtitle(spu, data, dataLen);
        if (ret != -1 && spu->buffer_size > 0) {
            SUBTITLE_LOGI("dump-pts-swdmx!success pts(%lld) frame was add\n", spu->pts);
            addDecodedItem(std::shared_ptr<AML_SPUVAR>(spu));
            notifySubtitleDimension(spu->spu_origin_display_w, spu->spu_origin_display_h);

            {
                std::unique_lock<std::mutex> autolock(mMutex);
                mPendingAction = 1;
                mCv.notify_all();
            }

        } else {
            SUBTITLE_LOGI("dump-pts-swdmx!error this pts(%lld) frame was abandon\n", spu->pts);
        }

        if (data) free(data);
    }
    return ret;
}

void DvbParser::callbackProcess() {
    int timeout = 60;
    #ifdef NEED_DECODE_TIMEOUT_ANDROID
    timeout = property_get_int32("vendor.sys.subtitleService.decodeTimeOut", 60);
    #elif NEED__DECODE_TIMEOUT_LINUX
    timeout = 60;
    #endif
    const int SIGNAL_UNSPEC = -1;
    const int NO_SIGNAL = 0;
    const int HAS_SIGNAL = 1;

    // three states flag.
    int signal = SIGNAL_UNSPEC;

    while (!mThreadExitRequested) {
        std::unique_lock<std::mutex> autolock(mMutex);
        mCv.wait_for(autolock, std::chrono::milliseconds(timeout*1000));

        // has pending action, this is new subtitle decoded.
        if (mPendingAction == 1) {
            if (signal != HAS_SIGNAL) {
                mNotifier->onSubtitleAvailable(1);
                signal = HAS_SIGNAL;
            }
            mPendingAction = -1;
            continue;
        }

        // timedout:
        if (signal != NO_SIGNAL) {
            signal = NO_SIGNAL;
            mNotifier->onSubtitleAvailable(0);
        }
    }
}

int DvbParser::getSpu() {
    if (mState == SUB_INIT) {
        mState = SUB_PLAYING;
    } else if (mState == SUB_STOP) {
        SUBTITLE_LOGI(" mState == SUB_STOP \n\n");
        return 0;
    }

    return getDvbSpu();
}

int DvbParser::getInterSpu() {
    return getSpu();
}

int DvbParser::parse() {
    while (!mThreadExitRequested) {
        if (getInterSpu() < 0) {
            // advance input, if parse failed, wait and retry.
            //std::this_thread::sleep_for(std::chrono::milliseconds(1));
            usleep(100);
        }
    }
    return 0;
}

void DvbParser::dump(int fd, const char *prefix) {
    dprintf(fd, "%s Dvb Parser\n", prefix);
    dumpCommon(fd, prefix);

    if (mContext != nullptr) {
        mContext->dump(fd, prefix);
    }

}

