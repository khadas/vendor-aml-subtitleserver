#ifndef AM_SUBTITLE_H
#define AM_SUBTITLE_H

#ifdef RDK_AML_SUBTITLE_SOCKET

#include "AmSocketClient.h"

//#include <media/stagefright/MediaBuffer.h>
//#include <media/stagefright/MetaData.h>
//#include "mediainfo.h"
#ifdef USE_SOCKETCLIENT_API
#include "SubtitleReportAPI.h"
#define SUB_HEADER_SIZE_NEW_ARCHITECUTRE 24
#endif
//namespace aml_mp {
//------------------------------------------------------------------------------
#define UNIT_FREQ   96000
#define PTS_FREQ    90000
#define str2ms(s) (((s[1]-0x30)*3600*10 \
                +(s[2]-0x30)*3600 \
                +(s[4]-0x30)*60*10 \
                +(s[5]-0x30)*60 \
                +(s[7]-0x30)*10 \
                +(s[8]-0x30))*1000 \
                +(s[10]-0x30)*100 \
                +(s[11]-0x30)*10 \
                +(s[12]-0x30))

/*
enum CodecID {
    // subtitle codecs
    CODEC_ID_DVD_SUBTITLE = 0x17000,
    CODEC_ID_DVB_SUBTITLE,
    CODEC_ID_TEXT,  ///< raw UTF-8 text
    CODEC_ID_XSUB,
    CODEC_ID_SSA,
    CODEC_ID_MOV_TEXT,
    CODEC_ID_HDMV_PGS_SUBTITLE,
    CODEC_ID_DVB_TELETEXT,
    CODEC_ID_SRT,

    CODEC_ID_MICRODVD = 0x17800,
    CODEC_ID_EIA_608,
    CODEC_ID_JACOSUB,
    CODEC_ID_SAMI,
    CODEC_ID_REALTEXT,
    CODEC_ID_STL,
    CODEC_ID_SUBVIEWER1,
    CODEC_ID_SUBVIEWER,
    CODEC_ID_SUBRIP,
    CODEC_ID_WEBVTT,
    CODEC_ID_MPL2,
    CODEC_ID_VPLAYER,
    CODEC_ID_PJS,
    CODEC_ID_ASS,
    CODEC_ID_HDMV_TEXT_SUBTITLE,
};
*/

class AmSubtitle
{
public:
    AmSubtitle();
    ~AmSubtitle();
    void init();
    void reset();
    void exit();
    void setSubFlag(bool flag);
    bool getSubFlag();
    void setStartPtsUpdateFlag(bool flag);
    bool getStartPtsUpdateFlag();
    void setTypeUpdateFlag(bool flag);
    bool getTypeUpdateFlag();
    void sendTime(int64_t timeUs);
    void setSubType(int session, int type);
    //void setSubStartPts(MediaBufferBase *mbuf);
    void setSubStartPts(int64_t pts);
    void setSubInfo(int total,const char * subTypeStr,const char* subLanStr);
    //void sendToSubtitleService(MediaBufferBase *mbuf);
    void sendToSubtitleService(const uint8_t *mbuf, size_t size, int64_t pts);

	int sendCmdToSubtitleService(const uint8_t *data, size_t length);
	int recvCmdFromSubtitleService(uint8_t *data, size_t length);
	int connectCmdSocket();
	int connectDataSocket();

private:
    bool mDebug;
    bool mDumpEnable;
#ifdef USE_SOCKETCLIENT_API
    SubSourceHandle mSubSourceHandle;
#else
    AmSocketClient *mClient;
#endif
    int mLastDuration;
    bool mIsAmSubtitle;
    bool mStartPtsUpdate;
    bool mTypeUpdate;
    int mSubType;
    int mSubTotal;
    char mSubTypeStr[1024] = {0};
    char mSubLanStr[1024] = {0};
};
//------------------------------------------------------------------------------
//}
#endif //RDK_AML_SUBTITLE_SOCKET
#endif
