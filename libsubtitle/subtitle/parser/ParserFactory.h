#ifndef _SUBTITLE_PARSER_FACTORY_H__
#define _SUBTITLE_PARSER_FACTORY_H__

#include "DataSource.h"
#include "Parser.h"

#define TYPE_SUBTITLE_Q_TONE_DATA 0xAAAA
#define SUPPORT_KOREA

enum ExtSubtitleType {
    SUB_INVALID   = -1,
    SUB_MICRODVD,
    SUB_SUBRIP,
    SUB_SUBVIEWER,
    SUB_SAMI,
    SUB_VPLAYER,  //4
    SUB_RT,
    SUB_SSA,      //6
    SUB_PJS,
    SUB_MPSUB,    //8
    SUB_AQTITLE,
    SUB_SUBVIEWER2,
    SUB_SUBVIEWER3,
    SUB_SUBRIP09,
    SUB_JACOSUB,
    SUB_MPL1,
    SUB_MPL2,
    SUB_XML,
    SUB_TTML,
    SUB_LRC,
    SUB_DIVX,
    SUB_WEBVTT,
    SUB_IDXSUB,
};



//from ffmpeg avcodec.h
enum SubtitleCodecID {
    /* subtitle codecs */
    AV_CODEC_ID_DVD_SUBTITLE = 0x17000,
    AV_CODEC_ID_DVB_SUBTITLE,
    AV_CODEC_ID_TEXT,  ///< raw UTF-8 text
    AV_CODEC_ID_XSUB,
    AV_CODEC_ID_SSA,
    AV_CODEC_ID_MOV_TEXT,
    AV_CODEC_ID_HDMV_PGS_SUBTITLE,
    AV_CODEC_ID_DVB_TELETEXT,
    AV_CODEC_ID_SRT,

    AV_CODEC_ID_VOB_SUBTITLE = 0x1700a, //the same as AV_CODEC_ID_DVD_SUBTITLE

    AV_CODEC_ID_MICRODVD   = 0x17800,
    AV_CODEC_ID_EIA_608,
    AV_CODEC_ID_JACOSUB,
    AV_CODEC_ID_SAMI,
    AV_CODEC_ID_REALTEXT,
    AV_CODEC_ID_STL,
    AV_CODEC_ID_SUBVIEWER1,
    AV_CODEC_ID_SUBVIEWER,
    AV_CODEC_ID_SUBRIP,
    AV_CODEC_ID_WEBVTT,
    AV_CODEC_ID_MPL2,
    AV_CODEC_ID_VPLAYER,
    AV_CODEC_ID_PJS,
    AV_CODEC_ID_ASS,
    AV_CODEC_ID_HDMV_TEXT_SUBTITLE,

};



enum DisplayType {
    SUBTITLE_IMAGE_DISPLAY       = 1,
    SUBTITLE_TEXT_DISPLAY,
};

typedef enum {
    DTV_SUB_INVALID = -1,
    DTV_SUB_CC              = 2,
    DTV_SUB_SCTE27          = 3,
    DTV_SUB_DVB                     = 4,
    DTV_SUB_DTVKIT_DVB         =5,
    DTV_SUB_DTVKIT_TELETEXT = 6,
    DTV_SUB_DTVKIT_SCTE27    = 7,
} DtvSubtitleType;

enum VideoFormat {
    INVALID_CC_TYPE    = -1,
    MPEG_CC_TYPE       = 0,
    H264_CC_TYPE       = 2,
    H265_CC_TYPE       = 2
};


typedef struct {
    int ChannelID = 0 ;
    int vfmt = 0;  //Video format
    char lang[64] = {0};  //channel language
} CcParam;


typedef struct {
    int SCTE27_PID = 0;
    int demuxId = 0;
} Scte27Param;

typedef struct {
   int demuxId = 0;
   int pid = 0;
   int compositionId = 0;
   int ancillaryId = 0;
}DtvKitDvbParam;

typedef struct {
   int demuxId = 0;
   int pid = 0;
   int magazine = 0;
   int page = 0;
}DtvKitTeletextParam;
typedef enum {
    CMD_INVALID        = -1,
    CMD_GO_HOME        = 1,
    CMD_GO_TO_PAGE     = 2,
    CMD_NEXT_PAGE      = 3,
    CMD_NEXT_SUB_PAGE  = 4,
} TeletextCtrlCmd;


typedef enum{
   TT_EVENT_INVALID          = -1,
   // These are the four FastText shortcuts, usually represented by red, green,
   // yellow and blue keys on the handset.
   TT_EVENT_QUICK_NAVIGATE_1 = 0,
   TT_EVENT_QUICK_NAVIGATE_2,
   TT_EVENT_QUICK_NAVIGATE_3,
   TT_EVENT_QUICK_NAVIGATE_4, //3//3
   // The ten numeric keys used to input page indexes.
   TT_EVENT_0,
   TT_EVENT_1,
   TT_EVENT_2,
   TT_EVENT_3,
   TT_EVENT_4,
   TT_EVENT_5,
   TT_EVENT_6,
   TT_EVENT_7,
   TT_EVENT_8,
   TT_EVENT_9,//13
   // This is the home key, which returns to the nominated index page for this
   //   service.
   TT_EVENT_INDEXPAGE,
   // These are used to quickly increment/decrement the page index.
   TT_EVENT_NEXTPAGE,
   TT_EVENT_PREVIOUSPAGE,
   // These are used to navigate the sub-pages when in 'hold' mode.
   TT_EVENT_NEXTSUBPAGE,
   TT_EVENT_PREVIOUSSUBPAGE,
   // These are used to traverse the page history (if caching requested).
   TT_EVENT_BACKPAGE,
   TT_EVENT_FORWARDPAGE, //20
   // This is used to toggle hold on the current page.
   TT_EVENT_HOLD,
   // Reveal hidden page content (as defined in EBU specification)
   TT_EVENT_REVEAL,
   // This key toggles 'clear' mode (page hidden until updated)
   TT_EVENT_CLEAR,
   // This key toggles 'clock only' mode (page hidden until updated)
   TT_EVENT_CLOCK,
   // Used to toggle transparent background ('video mix' mode)
   TT_EVENT_MIX_VIDEO, //25
   // Used to toggle double height top / double-height bottom / normal height display.
   TT_EVENT_DOUBLE_HEIGHT,
   // Functional enhancement may offer finer scrolling of double-height display.
   TT_EVENT_DOUBLE_SCROLL_UP,
   TT_EVENT_DOUBLE_SCROLL_DOWN,
   // Used to initiate/cancel 'timer' mode (clear and re-display page at set time)
   TT_EVENT_TIMER,
   TT_EVENT_GO_TO_PAGE,
   TT_EVENT_GO_TO_SUBTITLE,
   TT_EVENT_SET_REGION_ID
} TeletextEvent;



class TeletextParam {
public:
    int demuxId;
    int pid;
    int magazine;
    int page;
    int pageNo;
    int subPageNo;
    int pageDir;  //+1:next page, -1: last page
    int subPageDir;//+1:next sub page, -1: last sub page
    int regionId;
    TeletextCtrlCmd ctrlCmd;
    TeletextEvent event;
    // when play the same program, vbi is cached, must support quick reference page
    int onid; // origin network id for check need close vbi or not
    int tsid; // tsid. for check need close vbi or not
    TeletextParam() {
        demuxId = pid = magazine = page = pageNo = subPageNo = pageDir
            = subPageDir = regionId = -1;
        ctrlCmd = CMD_INVALID;
        event = TT_EVENT_INVALID;
        onid = tsid = -1;
    }
};

struct SubtitleParamType {
    SubtitleType subType;
    DtvSubtitleType dtvSubType;

    // TODO: maybe, use union for params
    Scte27Param scteParam;
    CcParam ccParam;
    TeletextParam ttParam;

    int renderType = 1;

    int playerId;
    int mediaId;

    int idxSubTrackId; // only for idxsub
    DtvKitDvbParam dtvkitDvbParam; //the pes pid for filter subtitle data from demux
    SubtitleParamType() : playerId(0), mediaId(0) {
        subType = TYPE_SUBTITLE_INVALID;
        memset(&ccParam, 0, sizeof(ccParam));
    }

    void update() {
        switch (dtvSubType) {
            case DTV_SUB_CC:
                subType = TYPE_SUBTITLE_CLOSED_CAPTION;
                break;
            case DTV_SUB_SCTE27:
                subType = TYPE_SUBTITLE_SCTE27;
                break;
            case DTV_SUB_DTVKIT_DVB:
                subType = TYPE_SUBTITLE_DTVKIT_DVB;
                break;
            case DTV_SUB_DTVKIT_TELETEXT:
                subType = TYPE_SUBTITLE_DTVKIT_TELETEXT;
                break;
            case DTV_SUB_DTVKIT_SCTE27:
                subType = TYPE_SUBTITLE_DTVKIT_SCTE27;
                break;
            default:
                break;
        }
    }

    bool isValidDtvParams () {
        return dtvSubType == DTV_SUB_CC || dtvSubType == DTV_SUB_SCTE27 ||dtvSubType == DTV_SUB_DTVKIT_SCTE27 ; //only cc or scte27 valid
    }

    void dump(int fd, const char * prefix) {
        dprintf(fd, "%s subType: %d\n", prefix, subType);
        dprintf(fd, "%s dtvSubType: %d\n", prefix, dtvSubType);
        dprintf(fd, "%s   SCTE27 (PID: %d)\n", prefix, scteParam.SCTE27_PID);
        dprintf(fd, "%s   CC     (ChannelID: %d vfmt: %d)\n", prefix, ccParam.ChannelID, ccParam.vfmt);
        dprintf(fd, "%s   TelTxt (PageNo: %d subPageNo: %d PageDir: %d subPageDir: %d ctlCmd: %d)\n",
            prefix, ttParam.pageNo, ttParam.subPageNo, ttParam.pageDir, ttParam.subPageDir, ttParam.ctrlCmd);
    }
};


class ParserFactory {

public:
    static std::shared_ptr<Parser> create(std::shared_ptr<SubtitleParamType>, std::shared_ptr<DataSource> source);
    static DisplayType getDisplayType(int type);

};


#endif
