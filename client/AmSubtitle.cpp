/************************************************
 * name : AmSubtitle.cpp
 * function : amlogic subtitle send managerment
 * data : 2017.04.20
 * author : wxl
 * version  : 1.0.0
 *************************************************/

#ifdef RDK_AML_SUBTITLE_SOCKET

#define LOG_NDEBUG 0
#define LOG_TAG "AmSubtitle"
#include <utils/Log.h>

#include <stdio.h>
#include <string.h>     //for strerror()
#include <errno.h>

#include "cutils/properties.h"
#include <inttypes.h>
#include <stdlib.h>
//#include <AmMetaDataExt.h>

#include "AmSubtitle.h"
//#include "utils/AmlMpLog.h"

const static unsigned int HEADER_SIZE = 4*5;
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
const static unsigned int START_FLAG = 0xF0D0C0B1;
const static unsigned int MAGIC_FLAG = 0xCFFFFFFB;

static const char* mName = "AmSubtitle";
//namespace aml_mp {

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

AmSubtitle::AmSubtitle() {
#ifdef USE_SOCKETCLIENT_API
    mSubSourceHandle = 0;
    mDumpEnable = false;
    mSubType = -1;
    mSubTotal = -1;
    mDebug = false;
#else
    mClient = NULL;
#endif
}

AmSubtitle::~AmSubtitle() {
#ifdef USE_SOCKETCLIENT_API
    if (mSubSourceHandle != 0) {
        SubSource_Stop(mSubSourceHandle);
        SubSource_Destroy(mSubSourceHandle);
        mSubSourceHandle = 0;
    }
#else
    if (mClient != NULL) {
        mClient->socketSend((char *)"exit\n", 5);
        mClient->socketDisconnect();
        delete mClient;
        mClient = NULL;
    }
#endif
}

void AmSubtitle::init() {
    mDebug = false;
    char value[PROPERTY_VALUE_MAX] = {0};
    if (property_get("sys.amsubtitle.debug", value, "false") > 0) {
        if (!strcmp(value, "true")) {
            mDebug = true;
        }
    }

    if (property_get("sys.amsubtitle.dump", value, "false") > 0) {
        if (!strcmp(value, "true") || !strcmp(value, "1")) {
            mDumpEnable = true;
        }
    }
    //ALOGI("sys.amsubtitle.dump = %s",value);

    if (mDebug) {
        //ALOGI("[init]\n");
    }

#ifdef USE_SOCKETCLIENT_API
    if (mSubSourceHandle == 0) {
        mSubSourceHandle = SubSource_Create(getpid());
    }
#else
    if (mClient == NULL) {
        mClient = new AmSocketClient();
        // socket may not ready due to 2 process synchr, checking and retry
        /*
		int retry = 100;
        while (retry-- >=0) {
            if (mClient->socketConnect() == 0) {
                break;
            }
            usleep(10*1000);
        }
		retry = 100;
        while (retry-- >=0) {
            if (mClient->socketConnectCmd() == 0) {
                break;
            }
            usleep(10*1000);
        }
		*/
    }
#endif
}

int AmSubtitle::connectDataSocket()
{
	int err = -1;
	if (mClient != NULL)
	{
        int retry = 100;
        while (retry-- >=0) {
            if (mClient->socketConnect() == 0) {
                err = 0;
				break;
            }
            usleep(10*1000);
        }
	}

	return err;
}

int AmSubtitle::connectCmdSocket()
{
	int err = -1;
	if (mClient != NULL)
	{
        int retry = 100;
        while (retry-- >=0) {
            if (mClient->socketConnectCmd() == 0) {
                err = 0;
				break;
            }
            usleep(10*1000);
        }
	}

	return err;
}

void AmSubtitle::exit() {
#ifdef USE_SOCKETCLIENT_API
    if (mSubSourceHandle != 0) {
        SubSource_Stop(mSubSourceHandle);
    }
#else
    if (mClient != NULL) {
        mClient->socketSend((char *)"exit\n", 5);
    }
#endif
}

void AmSubtitle::reset() {
#ifdef USE_SOCKETCLIENT_API
    if (mSubSourceHandle != 0) {
        SubSource_Reset(mSubSourceHandle);
    }
#else
    if (mClient != NULL) {
        mClient->socketSend((char *)"reset\n", 6);
        mTypeUpdate = false;
    }
#endif
}

void AmSubtitle::setSubFlag(bool flag) {
    mIsAmSubtitle = flag;
}

bool AmSubtitle::getSubFlag() {
    if (mDebug) {
        //ALOGI("[getSubFlag]mIsAmSubtitle:%d\n", (int)mIsAmSubtitle);
    }

    return mIsAmSubtitle;
}

void AmSubtitle::setStartPtsUpdateFlag(bool flag) {
    mStartPtsUpdate = flag;
}

bool AmSubtitle::getStartPtsUpdateFlag() {
    if (mDebug) {
        //ALOGI("[getStartPtsUpdateFlag]mStartPtsUpdate:%d\n", (int)mStartPtsUpdate);
    }

    return mStartPtsUpdate;
}

void AmSubtitle::setTypeUpdateFlag(bool flag) {
    mTypeUpdate = flag;
}

bool AmSubtitle::getTypeUpdateFlag() {
    if (mDebug) {
        //ALOGI("[getTypeUpdateFlag]mTypeUpdate:%d\n", (int)mTypeUpdate);
    }

    return mTypeUpdate;
}

void AmSubtitle::sendTime(int64_t timeUs) {
    if (mDebug) {
        //ALOGI("[sendTime]timeUs:%" PRId64"\n", timeUs);
    }

#ifdef USE_SOCKETCLIENT_API
    if (mSubSourceHandle != 0) {
        SubSource_ReportRenderTime(mSubSourceHandle, timeUs);
    }
#else
    char buf[8] = {0x53, 0x52, 0x44, 0x54};//SRDT //subtitle render time
    buf[4] = (timeUs >> 24) & 0xff;
    buf[5] = (timeUs >> 16) & 0xff;
    buf[6] = (timeUs >> 8) & 0xff;
    buf[7] = timeUs & 0xff;
    if (mClient != NULL) {
        mClient->socketSend(buf, 8);
    }
#endif
}

void AmSubtitle::setSubInfo(int total, const char* subTypeStr, const char* subLanStr) {
    if (mDebug) {
        //ALOGI("[setSubInfo]total:%d,subTypeStr:%s,subLanStr:%s\n", total,
        //subTypeStr, subLanStr);
    }

    int mSubStrLen = strlen(subTypeStr);
    memset(mSubTypeStr, 0, 1024);
    memcpy(mSubTypeStr,subTypeStr,mSubStrLen);
    mSubTypeStr[mSubStrLen] = '\0';

    mSubStrLen = strlen(subLanStr);
    memset(mSubLanStr, 0, 1024);
    memcpy(mSubLanStr,subLanStr,mSubStrLen);
    mSubLanStr[mSubStrLen] = '\0';

   // mSubTypeStr = subTypeStr;
  //  mSubLanStr = subLanStr;

    mSubTotal = total;
#ifdef USE_SOCKETCLIENT_API
    if (mSubSourceHandle != 0) {
        SubSource_ReportTotalTracks(mSubSourceHandle, mSubTotal);
        SubSource_ReportSubTypeString(mSubSourceHandle, subTypeStr);
        SubSource_ReportLauguageString(mSubSourceHandle, subLanStr);
    }
#else
    char buf[8] = {0x53, 0x54, 0x4F, 0x54};//STOT
    buf[4] = (total >> 24) & 0xff;
    buf[5] = (total >> 16) & 0xff;
    buf[6] = (total >> 8) & 0xff;
    buf[7] = total & 0xff;

    int size = 8 + strlen(subTypeStr) + 1 + strlen(subLanStr);
    char * data = (char *)malloc(size + 1);
    if (!data)
    {
        //ALOGE("setSubInfo malloc Failed!\n");
        return;
    }

    memset(data, 0, size + 1);
    memcpy(data, buf, 8);
    memcpy(data + 8, subTypeStr, strlen(subTypeStr));
    memcpy(data + 8 + strlen(subTypeStr), "/", 1);
    memcpy(data + 9 + strlen(subTypeStr), subLanStr, strlen(subLanStr) );
    data[size] = '\0';
    if (mClient != NULL) {
        mClient->socketSend(data, size);
    } else {
        //ALOGE("[setSubTotal]total:%d , mClient == NULL\n", total);
    }

    if (data != NULL) {
        free(data);
        data = NULL;
    }
#endif
}

    void AmSubtitle::setSubStartPts(int64_t pts) {
//void AmSubtitle::setSubStartPts(MediaBufferBase *mbuf) {
  //  if (mbuf == NULL) {
  //      return;
  //  }

    if (!mStartPtsUpdate) {
       // int64_t pts;
      //  mbuf->meta_data().findInt64(kKeyPktFirstVPts, &pts);
        if (pts >= 0) {
            if (mDebug) {
                //ALOGI("[setSubStartPts]pts:%" PRId64 "\n", pts);
            }

            mStartPtsUpdate = true;
#ifdef USE_SOCKETCLIENT_API
            if (mSubSourceHandle != 0) {
                // Check bellow, old implements. it should bug, only transfer 32bits
                // But, if changed to send all 64 bit contents, some subtitle not
                // display! strange! why parser more than 32 bits pts? if not work?
                SubSource_ReportStartPts(mSubSourceHandle, pts);
            }
#else
            char buf[8] = {0x53, 0x50, 0x54, 0x53};//SPTS
            buf[4] = (pts >> 24) & 0xff;
            buf[5] = (pts >> 16) & 0xff;
            buf[6] = (pts >> 8) & 0xff;
            buf[7] = pts & 0xff;
            if (mClient != NULL) {
                mClient->socketSend(buf, 8);
            } else {
                //ALOGE("[setSubStartPts]  mClient == NULL\n");
            }
#endif
        }
    }
}

void AmSubtitle::setSubType(int session, int type) {
  //  if (!mTypeUpdate) {
       mTypeUpdate = true;
        mSubType = type;
        if (mDebug) {
            //ALOGI("[setSubType]type:%d\n", type);
        }
	  char buffer[64];
        makeHeader(buffer, session, SUBTITLE_SUB_TYPE, sizeof(type));
        memcpy(buffer+HEADER_SIZE, &type, sizeof(type));
	  if (mClient != NULL) {
           mClient->socketSend(buffer, HEADER_SIZE+sizeof(type));
        } else {
            //ALOGE("[setSubStartPts]  mClient == NULL\n");
        }
}

int AmSubtitle::sendCmdToSubtitleService(const uint8_t *data, size_t length) {
	if (length <= 0)
	{
        return -1;
    }

	if (data == NULL)
	{
        return -1;
    }

	return mClient->socketSendCmd((const char*)data, length);
}

int AmSubtitle::recvCmdFromSubtitleService(uint8_t *data, size_t length) {
	if (length <= 0)
	{
        return -1;
    }

	if (data == NULL)
	{
        return -1;
    }

	return mClient->socketRecvCmd((char*)data, length);
}

void AmSubtitle::sendToSubtitleService(const uint8_t *mbuf, size_t length, int64_t pts){
    if (mbuf == NULL) {
        return;
    }
    unsigned int sub_type;
    float duration = 0;
    int64_t sub_pts = 0;
    int64_t start_time = 0;
    int data_size = length;
    if (length <= 0) {
            //MLOGI("[sendToSubtitleService]not enough data.data_size:%d\n", length);
        return;
    }
#if 0
    if (streamTimeBaseNum && (0 != streamTimeBaseDen)) {
        duration = PTS_FREQ * ((float)streamTimeBaseNum / streamTimeBaseDen);
        start_time = streamStartTime * streamTimeBaseNum * PTS_FREQ / streamTimeBaseDen;
        mLastDuration = 0;
    } else {
        start_time = streamStartTime * PTS_FREQ;
    }

    /* get pkt pts */
    if (0 != pktPts) {
        sub_pts = pktPts * duration;
        if (sub_pts < start_time) {
            sub_pts = sub_pts * mLastDuration;
        }
    } else if (0 != pktDts) {
        sub_pts = pktDts * duration * mLastDuration;
        mLastDuration = pktDuration;
    } else {
        sub_pts = 0;
    }
#endif
/*
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
    if (mDebug) {
        //ALOGE("[sendToSubtitleService]sub_type:0x%x, data_size:%d, sub_pts:%" PRId64 "\n", sub_type, data_size, pts);
    }
*/
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
        #if 0
        char *buf = (char *)mbuf->data();
        sub_pts = str2ms(buf) * 90;

        // add flag for xsub to indicate alpha
        unsigned int codec_tag = streamCodecTag;
        if (codec_tag == MKTAG('D','X','S','A')) {
            sub_header[4] = sub_header[4] | 0x01;
        }
        #endif
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
    if (mClient != NULL) {
        usleep(500);
         mClient->socketSend(data, size);
    } else {
         //MLOGI("[sendToSubtitleService]  mClient == NULL\n");
    }
  if (data != nullptr) {
      free(data);
      data = nullptr;
  }
}

//}
#endif //RDK_AML_SUBTITLE_SOCKET