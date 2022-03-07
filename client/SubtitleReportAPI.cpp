#define LOG_NDEBUG 0
#define LOG_TAG "SocketAPI"
#include <mutex>
#include <stdint.h>
#include <string.h>
#include "SubtitleReportAPI.h"
#include <SubtitleType.h>
#include <fcntl.h>
#include "SubtitleClientLinux.h"
#include "AmSocketClient.h"

int mLastDuration;
bool mIsAmSubtitle;
bool mStartPtsUpdate;
bool mTypeUpdate;
int mSubType;
int mSubTotal;
char mSubTypeStr[1024] = {0};
char mSubLanStr[1024] = {0};

typedef struct {
   SubtitleClientLinux* mRemote;
    std::unique_ptr<AmSocketClient> mSocketClient;
    int sId;
    std::mutex mLock;   // TODO: maybe we need global lock
} SubtitleReportContext;

typedef enum {
    SUBTITLE_TOTAL_TRACK    = 'STOT',
    SUBTITLE_START_PTS      = 'SPTS',
    SUBTITLE_RENDER_TIME    = 'SRDT',
    SUBTITLE_SUB_TYPE       = 'STYP',
    SUBTITLE_TYPE_STRING    = 'TPSR',
    SUBTITLE_LANG_STRING    = 'LGSR',
    SUBTITLE_SUB_DATA       = 'PLDT',

    SUBTITLE_RESET_SERVER   = 'CDRT',
    SUBTITLE_EXIT_SERVER    = 'CDEX',

} PayloadType;

/**
    payload is:
        startFlag   : 4bytes
        sessionID   : 4Bytes (TBD)
        magic       : 4bytes (for double confirm)
        payload size: 4Bytes (indicate the size of payload, size is total_send_bytes - 4*5)
                      exclude this and above 4 items.
        payload Type: defined in PayloadType_t
        payload data: TO BE PARSED data
 */

#define LOGIT 1
const static unsigned int START_FLAG = 0xF0D0C0B1;
const static unsigned int MAGIC_FLAG = 0xCFFFFFFB;

const static unsigned int HEADER_SIZE = 4*5;

static void inline makeHeader(char *buf, int sessionId, PayloadType t, int payloadSize) {
    unsigned int val = START_FLAG;
    memcpy(buf, &val, sizeof(val));

    memcpy(buf+sizeof(int), &sessionId, sizeof(int));

    val = MAGIC_FLAG; // magic
    memcpy(buf+2*sizeof(int), &val, sizeof(int));

    memcpy(buf+3*sizeof(int), &payloadSize, sizeof(int));

    val = t;
    memcpy(buf+4*sizeof(val), &val, sizeof(val));
}

static void socketSendData(SubtitleReportContext *ctx, const char *mbuf, size_t length, int64_t pts){
    if (mbuf == NULL) {
        return;
    }
    unsigned int sub_type;
    float duration = 0;
    int64_t sub_pts = 0;
    int64_t start_time = 0;
    int data_size = length;
    if (length <= 0) {
            ALOGE("[sendToSubtitleService]not enough data.data_size:%d\n", length);
        return;
    }
    sub_type = CODEC_ID_DVB_SUBTITLE;//streamCodecID;
    if (sub_type == CODEC_ID_DVD_SUBTITLE) {
        mSubType = 0;
    } else if (sub_type == CODEC_ID_HDMV_PGS_SUBTITLE) {
        mSubType = 1;
    } else if (sub_type == CODEC_ID_XSUB) {
        mSubType = 2;
    } else if (sub_type == CODEC_ID_TEXT
        || sub_type == CODEC_ID_SSA
        || sub_type == CODEC_ID_ASS) {
        mSubType = 3;
    } else if (sub_type == CODEC_ID_DVB_SUBTITLE) {
        mSubType = 5;
    } else if (sub_type == 0x17005) {
        mSubType = 7;
    }  else if (sub_type == CODEC_ID_DVB_TELETEXT) {
        mSubType = 9; //SUBTITLE_DVB_TELETEXT
    } else {
        mSubType = 4;
    }
   // if (mDebug) {
   //     ALOGE("[sendToSubtitleService]sub_type:0x%x, data_size:%d, sub_pts:%" PRId64 "\n", sub_type, data_size, pts);
  //  }

    if (sub_type == 0x17000) {
        sub_type = 0x1700a;
    }
    else if (sub_type == 0x17002) {
        mLastDuration = 0;//(unsigned)pktConvergenceDuration * 90;
    }
    //0x1780d:mkv internel  ass  0x17808:internal UTF-8
    else if (sub_type == 0x1780d || sub_type == 0x17808) {
        mLastDuration = 0;//(unsigned)pktDuration * 90;     //time(ms) convert to pts, need   *90
    }
    else if (sub_type == 0x17003) {
        return;                                   //not need to support xsub
    }
    unsigned char sub_header[24] = {0x41, 0x4d, 0x4c, 0x55, 0x77, 0};
   //unsigned char sub_header[24] = {0};
   //unsigned char subInfo_header[10] = {0};   //add info header mSubType, mSubTotal send with subtitle packet.
    sub_header[5] = (sub_type >> 16) & 0xff;
    sub_header[6] = (sub_type >> 8) & 0xff;
    sub_header[7] = sub_type & 0xff;
    sub_header[8] = (data_size >> 24) & 0xff;
    sub_header[9] = (data_size >> 16) & 0xff;
    sub_header[10] = (data_size >> 8) & 0xff;
    sub_header[11] = data_size & 0xff;
    sub_header[12] = (pts >> 56) & 0xff;
    sub_header[13] = (pts >> 48) & 0xff;
    sub_header[14] = (pts >> 40) & 0xff;
    sub_header[15] = (pts >> 32) & 0xff;
    sub_header[16] = (pts >> 24) & 0xff;
    sub_header[17] = (pts >> 16) & 0xff;
    sub_header[18] = (pts >> 8) & 0xff;
    sub_header[19] = pts & 0xff;
    sub_header[20] = (mLastDuration >> 24) & 0xff;
    sub_header[21] = (mLastDuration >> 16) & 0xff;
    sub_header[22] = (mLastDuration >> 8) & 0xff;
    sub_header[23] = mLastDuration & 0xff;
    int size = 24 + data_size + 5*4;
    int data_length = 24+data_size;
    char * data = (char *)malloc(size);
    memset(data, 0, size );
    int sessionId = 1;
    unsigned int val = START_FLAG;
    memcpy(data, &val, sizeof(val));
    memcpy(data+1*sizeof(int), &sessionId, sizeof(int));
     int val1 = MAGIC_FLAG; // magic
    memcpy(data+2*sizeof(int), &val1, sizeof(int));
   memcpy(data+3*sizeof(int), &data_length, sizeof(int));
   int type =SUBTITLE_SUB_DATA;
    memcpy(data+4*sizeof(int), &type, sizeof(int));
    memcpy(data + 5*sizeof(int), sub_header, 24);
    memcpy(data + 5*sizeof(int) + 24, mbuf, data_size);
    if (ctx->mSocketClient != NULL) {
        usleep(500);
         ctx->mSocketClient->socketSend(data, size);
    } else {
         ALOGE("[sendToSubtitleService]  mClient == NULL\n");
    }
  if (data != nullptr) {
      free(data);
      data = nullptr;
  }
}


// TODO: move all the report API to subtitlebinder
SubSourceHandle SubSource_Create(int sId) {
    SubtitleReportContext *ctx = new SubtitleReportContext();
    if (ctx == nullptr) return nullptr;
    if (LOGIT) ALOGD("SubSource Create %d", sId);
    ctx->mLock.lock();
    ALOGD("ctx->mSocketClient: %p", ctx->mSocketClient.get());
    if (ctx->mSocketClient == NULL) {
        std::unique_ptr<AmSocketClient> pTemp(new AmSocketClient);
        ctx->mSocketClient = std::move(pTemp);//unique_ptr<AmSocketClient>;//new AmSocketClient();
        // socket may not ready due to 2 process synchr, checking and retry
        int retry = 100;
        while (retry-- >= 0) {
            ALOGD("socketConnect..., %d", retry);
            if (ctx->mSocketClient->socketConnect() == 0) {
                break;
            }
            usleep(10 * 1000);
        }
    }
    ctx->sId = sId;
    ctx->mLock.unlock();

    return ctx;
}

SubSourceStatus SubSource_Destroy(SubSourceHandle handle) {
    SubtitleReportContext *ctx = (SubtitleReportContext *) handle;
    if (ctx == nullptr) return SUB_STAT_INV;

    if (LOGIT) ALOGD("SubSource destroy %d", ctx->sId);

    ctx->mLock.lock();
    // ctx->mRemote = nullptr;
    ctx->mSocketClient->socketDisconnect();
    ctx->mSocketClient.reset();
    ctx->mLock.unlock();

    delete ctx;
    return SUB_STAT_OK;
}


SubSourceStatus SubSource_Reset(SubSourceHandle handle) {
    SubtitleReportContext *ctx = (SubtitleReportContext *) handle;
    if (ctx == nullptr) return SUB_STAT_INV;

    if (LOGIT) ALOGD("SubSource reset %d", ctx->sId);

    std::lock_guard<std::mutex> guard(ctx->mLock);

    char buffer[64];
    memset(buffer, 0, 64);
    makeHeader(buffer, ctx->sId, SUBTITLE_RESET_SERVER, 7);
    strcpy(buffer+HEADER_SIZE, "reset\n");
        ctx->mSocketClient->socketSend(buffer, HEADER_SIZE+7);

    return SUB_STAT_OK;
}

SubSourceStatus SubSource_Stop(SubSourceHandle handle) {
    SubtitleReportContext *ctx = (SubtitleReportContext *) handle;
    if (ctx == nullptr) return SUB_STAT_INV;

    if (LOGIT) ALOGD("SubSource destroy %d", ctx->sId);
    std::lock_guard<std::mutex> guard(ctx->mLock);
    char buffer[64];
    memset(buffer, 0, 64);
    makeHeader(buffer, ctx->sId, SUBTITLE_EXIT_SERVER, 6);
    strcpy(buffer+HEADER_SIZE, "exit\n");
        ctx->mSocketClient->socketSend(buffer, HEADER_SIZE+6);
    return SUB_STAT_OK;

}

SubSourceStatus SubSource_ReportRenderTime(SubSourceHandle handle, int64_t timeUs) {
    SubtitleReportContext *ctx = (SubtitleReportContext *) handle;
    static int count = 0;
    if (ctx == nullptr) return SUB_STAT_INV;

    if (LOGIT) {
        if (count++ %1000 == 0) ALOGD("SubSource ReportRenderTime %d 0x%llx", ctx->sId, timeUs);
    }

    std::lock_guard<std::mutex> guard(ctx->mLock);
    char buffer[64];
    makeHeader(buffer, ctx->sId, SUBTITLE_RENDER_TIME, sizeof(int64_t));
    memcpy(buffer+HEADER_SIZE, &timeUs, sizeof(int64_t));
    ctx->mSocketClient->socketSend(buffer, HEADER_SIZE+sizeof(int64_t));
    return SUB_STAT_OK;
}

SubSourceStatus SubSource_ReportStartPts(SubSourceHandle handle, int64_t pts) {
    SubtitleReportContext *ctx = (SubtitleReportContext *) handle;
    if (ctx == nullptr) return SUB_STAT_INV;

    if (LOGIT) ALOGD("SubSource ReportStartPts %d 0x%llx", ctx->sId, pts);

    std::lock_guard<std::mutex> guard(ctx->mLock);
    char buffer[64];
    makeHeader(buffer, ctx->sId, SUBTITLE_START_PTS, sizeof(pts));
    memcpy(buffer+HEADER_SIZE, &pts, sizeof(pts));
    ctx->mSocketClient->socketSend(buffer, HEADER_SIZE+sizeof(pts));
    return SUB_STAT_OK;
}

SubSourceStatus SubSource_ReportTotalTracks(SubSourceHandle handle, int trackNum) {
    SubtitleReportContext *ctx = (SubtitleReportContext *) handle;
    if (ctx == nullptr) return SUB_STAT_INV;

    if (LOGIT) ALOGD("SubSource ReportTotalTracks %d 0x%x", ctx->sId, trackNum);

    std::lock_guard<std::mutex> guard(ctx->mLock);
    char buffer[64];
    makeHeader(buffer, ctx->sId, SUBTITLE_TOTAL_TRACK, sizeof(trackNum));
    memcpy(buffer+HEADER_SIZE, &trackNum, sizeof(trackNum));
    ctx->mSocketClient->socketSend(buffer, HEADER_SIZE+sizeof(trackNum));
    return SUB_STAT_OK;
}


SubSourceStatus SubSource_ReportType(SubSourceHandle handle, int type) {
    SubtitleReportContext *ctx = (SubtitleReportContext *) handle;
    if (ctx == nullptr) return SUB_STAT_INV;

    if (LOGIT) ALOGD("SubSource ReportType %d 0x%x", ctx->sId, type);

    std::lock_guard<std::mutex> guard(ctx->mLock);
    char buffer[64];
    makeHeader(buffer, ctx->sId, SUBTITLE_SUB_TYPE, sizeof(type));
    memcpy(buffer+HEADER_SIZE, &type, sizeof(type));
    ctx->mSocketClient->socketSend(buffer, HEADER_SIZE+sizeof(type));

    return SUB_STAT_OK;
}


SubSourceStatus SubSource_ReportSubTypeString(SubSourceHandle handle, const char *type) {
    SubtitleReportContext *ctx = (SubtitleReportContext *) handle;
    std::lock_guard<std::mutex> guard(ctx->mLock);
    if (ctx == nullptr) return SUB_STAT_INV;

    char *buffer = new char[strlen(type)+1+HEADER_SIZE]();
    if (buffer == nullptr) return SUB_STAT_INV;

    if (LOGIT) ALOGD("SubSource ReportTypeString %d %s", ctx->sId, type);

    makeHeader(buffer, ctx->sId, SUBTITLE_TYPE_STRING, strlen(type)+1);
    memcpy(buffer+HEADER_SIZE, type, strlen(type));
    ctx->mSocketClient->socketSend(buffer, HEADER_SIZE+sizeof(type)+1);
    delete[] buffer;
    return SUB_STAT_OK;
}
SubSourceStatus SubSource_ReportLauguageString(SubSourceHandle handle, const char *lang) {
    SubtitleReportContext *ctx = (SubtitleReportContext *) handle;
    std::lock_guard<std::mutex> guard(ctx->mLock);
    if (ctx == nullptr) return SUB_STAT_INV;
    if (LOGIT) ALOGD("SubSource ReportLangString %d %s", ctx->sId, lang);

    char *buffer = new char[strlen(lang)+1+HEADER_SIZE]();
    if (buffer == nullptr) return SUB_STAT_INV;

    makeHeader(buffer, ctx->sId, SUBTITLE_LANG_STRING, strlen(lang)+1);
    memcpy(buffer+HEADER_SIZE, lang, strlen(lang));
    ctx->mSocketClient->socketSend(buffer, HEADER_SIZE+strlen(lang)+1);
    delete[] buffer;
    return SUB_STAT_OK;
}

SubSourceStatus SubSource_SendData(SubSourceHandle handle, const char *mbuf, int length, int64_t pts, enum CodecID sub_type) {
    SubtitleReportContext *ctx = (SubtitleReportContext *) handle;
    if (ctx == nullptr) return SUB_STAT_INV;
    if (LOGIT) ALOGD("SubSource SubSource_SendData %d %d", ctx->sId, length);

    std::lock_guard<std::mutex> guard(ctx->mLock);
    float duration = 0;
    int64_t sub_pts = 0;
    int64_t start_time = 0;
    int data_size = length;
    if (length <= 0) {
            ALOGE("[sendToSubtitleService]not enough data.data_size:%d\n", length);
        return SUB_STAT_INV;
    }
//    sub_type = CODEC_ID_DVB_SUBTITLE;//streamCodecID;
    if (sub_type == CODEC_ID_DVD_SUBTITLE) {
        mSubType = 0;
    } else if (sub_type == CODEC_ID_HDMV_PGS_SUBTITLE) {
        mSubType = 1;
    } else if (sub_type == CODEC_ID_XSUB) {
        mSubType = 2;
    } else if (sub_type == CODEC_ID_TEXT
        || sub_type == CODEC_ID_SSA
        || sub_type == CODEC_ID_ASS) {
        mSubType = 3;
    } else if (sub_type == CODEC_ID_DVB_SUBTITLE) {
        mSubType = 5;
    } else if (sub_type == CODEC_ID_MOV_TEXT) {
        mSubType = 7;
    }  else if (sub_type == CODEC_ID_DVB_TELETEXT) {
        mSubType = 9; //SUBTITLE_DVB_TELETEXT
    } else {
        mSubType = 4;
    }
   // if (mDebug) {
   //    ALOGE("[sendToSubtitleService]sub_type:0x%x, data_size:%d, sub_pts:%" PRId64 "\n", sub_type, data_size, pts);
   // }

    if (sub_type == CODEC_ID_DVD_SUBTITLE) {
        sub_type = static_cast<CodecID>(0x1700a); // Invalid
    }
    else if (sub_type == CODEC_ID_TEXT) {
        mLastDuration = 0;//(unsigned)pktConvergenceDuration * 90;
    }
    //0x1780d:mkv internel  ass  0x17808:internal UTF-8
    else if (sub_type == CODEC_ID_ASS || sub_type == CODEC_ID_SUBRIP) {
        mLastDuration = 0;//(unsigned)pktDuration * 90;     //time(ms) convert to pts, need   *90
    }
    else if (sub_type == CODEC_ID_XSUB) {
        return SUB_STAT_INV;                                   //not need to support xsub
    }
    unsigned char sub_header[24] = {0x41, 0x4d, 0x4c, 0x55, 0x77, 0};
   //unsigned char sub_header[24] = {0};
   //unsigned char subInfo_header[10] = {0};   //add info header mSubType, mSubTotal send with subtitle packet.
    sub_header[5] = (sub_type >> 16) & 0xff;
    sub_header[6] = (sub_type >> 8) & 0xff;
    sub_header[7] = sub_type & 0xff;
    sub_header[8] = (data_size >> 24) & 0xff;
    sub_header[9] = (data_size >> 16) & 0xff;
    sub_header[10] = (data_size >> 8) & 0xff;
    sub_header[11] = data_size & 0xff;
    sub_header[12] = (pts >> 56) & 0xff;
    sub_header[13] = (pts >> 48) & 0xff;
    sub_header[14] = (pts >> 40) & 0xff;
    sub_header[15] = (pts >> 32) & 0xff;
    sub_header[16] = (pts >> 24) & 0xff;
    sub_header[17] = (pts >> 16) & 0xff;
    sub_header[18] = (pts >> 8) & 0xff;
    sub_header[19] = pts & 0xff;
    sub_header[20] = (mLastDuration >> 24) & 0xff;
    sub_header[21] = (mLastDuration >> 16) & 0xff;
    sub_header[22] = (mLastDuration >> 8) & 0xff;
    sub_header[23] = mLastDuration & 0xff;
    int size = 24 + data_size + 5*4;
    int data_length = 24+data_size;
    char * data = (char *)malloc(size);
    memset(data, 0, size );

    ALOGD("SubSource_SendData, data_length= %d", data_length);
    makeHeader(data, ctx->sId, SUBTITLE_SUB_DATA, data_length);

    /*int sessionId = ctx->sId;
    unsigned int val = START_FLAG;
    memcpy(data, &val, sizeof(val));
    memcpy(data+1*sizeof(int), &sessionId, sizeof(int));
     int val1 = MAGIC_FLAG; // magic
    memcpy(data+2*sizeof(int), &val1, sizeof(int));
   memcpy(data+3*sizeof(int), &data_length, sizeof(int));
   int type =SUBTITLE_SUB_DATA;
    memcpy(data+4*sizeof(int), &type, sizeof(int));*/

    memcpy(data + 5*sizeof(int), sub_header, 24);
    memcpy(data + 5*sizeof(int) + 24, mbuf, data_size);
    if (ctx->mSocketClient != NULL) {
        usleep(500);
         ctx->mSocketClient->socketSend(data, size);
    } else {
         ALOGE("[sendToSubtitleService]  mClient == NULL\n");
    }
    if (data != nullptr) {
      free(data);
      data = nullptr;
    }
    if (LOGIT) ALOGD("SubSource SubSource_SendData end %d %d", ctx->sId, size);

    return SUB_STAT_OK;
}

//}
