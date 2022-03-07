#ifndef __SUBTITLE_SOCKET_CLIENT_API_H__
#define __SUBTITLE_SOCKET_CLIENT_API_H__
#include <map>
#include <memory>
#include "SubtitleAPICommon.h"

#include <binder/Binder.h>
#include <binder/Parcel.h>
#include <binder/IServiceManager.h>

using namespace android;

#ifdef __cplusplus
extern "C" {
#endif

typedef void *SubSourceHandle;

//typedef enum {
//    SUB_STAT_INV = -1,
//    SUB_STAT_FAIL = 0,
//    SUB_STAT_OK,
//}SubSourceStatus;

enum CodecID {
    /* subtitle codecs */
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

SubSourceHandle SubSource_Create(int sId);
SubSourceStatus SubSource_Destroy(SubSourceHandle handle);

SubSourceStatus SubSource_Reset(SubSourceHandle handle);
SubSourceStatus SubSource_Stop(SubSourceHandle handle);

SubSourceStatus SubSource_ReportRenderTime(SubSourceHandle handle, int64_t timeUs);
SubSourceStatus SubSource_ReportStartPts(SubSourceHandle handle, int64_t type);

SubSourceStatus SubSource_ReportTotalTracks(SubSourceHandle handle, int trackNum);
SubSourceStatus SubSource_ReportType(SubSourceHandle handle, int type);

SubSourceStatus SubSource_ReportSubTypeString(SubSourceHandle handle, const char *type);
SubSourceStatus SubSource_ReportLauguageString(SubSourceHandle handle, const char *lang);


//SubSourceStatus SubSource_SendData(SubSourceHandle handle, const char *data, int size);
SubSourceStatus SubSource_SendData(SubSourceHandle handle, const char *mbuf, int length, int64_t pts,
        enum CodecID sub_type = CODEC_ID_DVB_SUBTITLE) ;


#ifdef __cplusplus
}
#endif


#endif
