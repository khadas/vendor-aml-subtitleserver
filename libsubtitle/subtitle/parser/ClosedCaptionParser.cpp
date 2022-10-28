#define LOG_TAG "ClosedCaptionParser"
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <list>
#include <thread>
#include <algorithm>
#include <functional>
#include <mutex>
//#ifdef ANDROID
#include <utils/Log.h>
#include <utils/CallStack.h>
//#endif
//#include "trace_support.h"
#include <future>

#include "sub_types2.h"
#include "ClosedCaptionParser.h"
#include "ParserFactory.h"
#include "VideoInfo.h"



#define CC_EVENT_VCHIP_AUTH -1
#define CC_EVENT_VCHIP_FLAG -2
#define CC_EVENT_CHANNEL_ADD 1
#define CC_EVENT_CHANNEL_REMOVE 0

// we want access Parser in little-dvb callback.
// must use a global lock
static std::mutex gLock;
static char sJsonStr[CC_JSON_BUFFER_SIZE];


static void cc_report_cb(AM_CC_Handle_t handle, int error) {
    (void)handle;
    ClosedCaptionParser *parser = ClosedCaptionParser::getCurrentInstance();
    if (parser != nullptr) {
        parser->notifyAvailable(__notifierErrorRemap(error));
    }
}

struct ChannelHandle {
    int sendNoSig(int timeout) {
        while (!mReqStop && timeout-->0) {
            usleep(100);
        }
        {   // here, send notify
            std::unique_lock<std::mutex> autolock(gLock);
            ClosedCaptionParser *parser = ClosedCaptionParser::getCurrentInstance();
            if (parser != nullptr) {
                parser->notifyChannelState(0, 0);
            }
        }
        return 0;
    }
    bool mReqStop;
    int mBackupMask;
};

ChannelHandle gHandle = {true, 0};
std::thread gTimeoutThread;

static void cc_rating_cb (AM_CC_Handle_t handle, vbi_rating *rating) {
    (void)handle;
    std::unique_lock<std::mutex> autolock(gLock);
    ClosedCaptionParser *parser = ClosedCaptionParser::getCurrentInstance();
    if (parser != nullptr) {
        int data = 0;
        data = rating->auth<<16|rating->id<<8| rating->dlsv;
        ALOGD("CC_RATING_CB: auth: %d id:%d dlsv:%d", rating->auth, rating->id, rating->dlsv);
        parser->notifyChannelState(CC_EVENT_VCHIP_AUTH, data);
    }
}

static void q_tone_data_cb(AM_CC_Handle_t handle, char *buffer, int size) {
    ALOGD("q_tone_data_cb data:%s, size:%d", buffer ,size);
    std::shared_ptr<AML_SPUVAR> spu(new AML_SPUVAR());
    spu->subtitle_type = TYPE_SUBTITLE_CLOSED_CAPTION;
    spu->spu_data = (unsigned char *)malloc(size);
    memset(spu->spu_data, 0, size);
    memcpy(spu->spu_data, buffer, size);
    spu->buffer_size = size;
    spu->isQtoneData = true;

    // report data:
    {
        std::unique_lock<std::mutex> autolock(gLock);
        ClosedCaptionParser *parser = ClosedCaptionParser::getCurrentInstance();
        if (parser != nullptr) {
            // CC and scte use little dvb, render&presentation already handled.
            spu->isImmediatePresent = true;
            parser->addDecodedItem(std::shared_ptr<AML_SPUVAR>(spu));
        } else {
            ALOGD("Report json string to a deleted cc parser!");
        }
    }


}

static void cc_data_cb(AM_CC_Handle_t handle, int mask) {
    (void)handle;
    if (mask > 0) {
        gHandle.mReqStop = true; // todo: CV notify
        ClosedCaptionParser *parser = ClosedCaptionParser::getCurrentInstance();
        if (parser != nullptr) {
            parser->notifyChannelState(CC_EVENT_VCHIP_FLAG, mask);
        }
        ALOGD("CC_DATA_CB: mask: %d lastMask:%d", mask, gHandle.mBackupMask);
        for (int i = 0; i<15; i++) {
            unsigned int curr = (mask >> i) & 0x1;
            unsigned int last = (gHandle.mBackupMask >> i) & 0x1;
            ALOGD("CC_DATA_CB: curr: %d last:%d,index:%d", curr, last,i);

            if (curr != last) {
                std::unique_lock<std::mutex> autolock(gLock);
                if (parser != nullptr) {
                    parser->notifyChannelState((curr == 1) ? CC_EVENT_CHANNEL_ADD:CC_EVENT_CHANNEL_REMOVE, i);
                }
            }
        }
        gHandle.mBackupMask = mask;
    } else {
        gHandle.mReqStop = false;
        gTimeoutThread = std::thread(&ChannelHandle::sendNoSig, gHandle, 50000);
        gTimeoutThread.detach();
    }
}



void json_update_cb(AM_CC_Handle_t handle) {
    (void)handle;

    LOGI("@@@@@@ cc json string: %s", sJsonStr);
    int mJsonLen = strlen(sJsonStr);
    std::shared_ptr<AML_SPUVAR> spu(new AML_SPUVAR());
    spu->subtitle_type = TYPE_SUBTITLE_CLOSED_CAPTION;
    spu->spu_data = (unsigned char *)malloc(mJsonLen);
    memset(spu->spu_data, 0, mJsonLen);
    memcpy(spu->spu_data, sJsonStr, mJsonLen);
    spu->buffer_size = mJsonLen;

    // report data:
    {
        std::unique_lock<std::mutex> autolock(gLock);
        ClosedCaptionParser *parser = ClosedCaptionParser::getCurrentInstance();
        if (parser != nullptr) {
            // CC and scte use little dvb, render&presentation already handled.
            spu->isImmediatePresent = true;
            parser->addDecodedItem(std::shared_ptr<AML_SPUVAR>(spu));
        } else {
            ALOGD("Report json string to a deleted cc parser!");
        }
    }
    //saveJsonStr(ClosedCaptionParser::gJsonStr);
}

void ClosedCaptionParser::notifyAvailable(int avil) {
    if (mNotifier != nullptr) {
        mNotifier->onSubtitleAvailable(avil);
    }
}

void ClosedCaptionParser::notifyChannelState(int stat, int channelId) {
    if (mNotifier != nullptr) {
        ALOGD("CC_DATA_CB: %d %d", stat, channelId);
        mNotifier->onSubtitleDataEvent(stat, channelId);
    }
}


ClosedCaptionParser *ClosedCaptionParser::sInstance = nullptr;

ClosedCaptionParser *ClosedCaptionParser::getCurrentInstance() {
    return ClosedCaptionParser::sInstance;
}

ClosedCaptionParser::ClosedCaptionParser(std::shared_ptr<DataSource> source) {
    LOGI("creat ClosedCaption parser");
    mDataSource = source;
    mParseType = TYPE_SUBTITLE_CLOSED_CAPTION;

    std::unique_lock<std::mutex> autolock(gLock);
    sInstance = this;
    mCcContext = new TVSubtitleData();
}

ClosedCaptionParser::~ClosedCaptionParser() {
    LOGI("%s", __func__);
    {
        std::unique_lock<std::mutex> autolock(gLock);
        sInstance = nullptr;
    }
    stopAmlCC();
    // call back may call parser, parser later destroy
    stopParse();
    if (mLang) free(mLang);
    delete mCcContext;
}

bool ClosedCaptionParser::updateParameter(int type, void *data) {
    (void)type;
  CcParam *cc_param = (CcParam *) data;
  mChannelId = cc_param->ChannelID;
  if (strlen(cc_param->lang) > 0) {
      mLang = strdup(cc_param->lang);
  }
  //mVfmt = cc_param->vfmt;
  LOGI("@@@@@@ updateParameter mChannelId: %d, mVfmt:%d", mChannelId, mVfmt);
  return true;
}

void ClosedCaptionParser::setPipId (int mode, int id) {
    if (PIP_PLAYER_ID == mode) {
        if (id == mPlayerId)
            return;
        mPlayerId = id;
    } else if (PIP_MEDIASYNC_ID == mode) {
        mMediaSyncId = id;
    }
}

void ClosedCaptionParser::setDvbDebugLogLevel() {
    //AM_DebugSetLogLevel(property_get_int32("tv.dvb.loglevel", 1));
}



int ClosedCaptionParser::startAtscCc(int source, int vfmt, int caption, int fg_color,
        int fg_opacity, int bg_color, int bg_opacity, int font_style, int font_size)
{
    AM_CC_CreatePara_t cc_para;
    AM_CC_StartPara_t spara;
    int ret;
    int mode;

    setDvbDebugLogLevel();

    LOGI("start cc: vfmt %d caption %d, fgc %d, bgc %d, fgo %d, bgo %d, fsize %d, fstyle %d mMediaSyncId:%d",
            vfmt, caption, fg_color, bg_color, fg_opacity, bg_opacity, font_size, font_style, mMediaSyncId);

    memset(&cc_para, 0, sizeof(cc_para));
    memset(&spara, 0, sizeof(spara));

    cc_para.bmp_buffer = mCcContext->buffer;
    cc_para.pitch = mCcContext->bmp_pitch;
    //cc_para.draw_begin = cc_draw_begin_cb;
    //cc_para.draw_end = cc_draw_end_cb;
    cc_para.user_data = (void *)mCcContext;
    cc_para.input = (AM_CC_Input_t)source;
    //cc_para.rating_cb = cc_rating_cb;
    cc_para.data_cb = cc_data_cb;
    cc_para.report = cc_report_cb;
    cc_para.rating_cb = cc_rating_cb;
    cc_para.data_timeout = 5000;//5s
    cc_para.switch_timeout = 3000;//3s
    cc_para.json_update = json_update_cb;
    cc_para.json_buffer = sJsonStr;
#ifdef SUPPORT_KOREA
    cc_para.q_tone_cb = q_tone_data_cb;
    if (mLang != nullptr) {
        strncpy(cc_para.lang, mLang, sizeof(cc_para.lang));
        cc_para.lang[ sizeof(cc_para.lang) -1 ] = '\0';
    }
#endif

    spara.vfmt = vfmt;
    spara.player_id = mPlayerId;
    spara.mediaysnc_id = mMediaSyncId;
    spara.caption1                 = (AM_CC_CaptionMode_t)caption;
    spara.caption2                 = AM_CC_CAPTION_NONE;
    spara.user_options.bg_color    = (AM_CC_Color_t)bg_color;
    spara.user_options.fg_color    = (AM_CC_Color_t)fg_color;
    spara.user_options.bg_opacity  = (AM_CC_Opacity_t)bg_opacity;
    spara.user_options.fg_opacity  = (AM_CC_Opacity_t)fg_opacity;
    spara.user_options.font_size   = (AM_CC_FontSize_t)font_size;
    spara.user_options.font_style  = (AM_CC_FontStyle_t)font_style;

    ALOGD("%s %s", mLang, cc_para.lang);
    ret = AM_CC_Create(&cc_para, &mCcContext->cc_handle);
    if (ret != AM_SUCCESS) {
        goto error;
    }

    ret = AM_CC_Start(mCcContext->cc_handle, &spara);
    if (ret != AM_SUCCESS) {
        goto error;
    }
    LOGI("start cc successfully!");
    return 0;
error:
    if (mCcContext->cc_handle != NULL) {
        AM_CC_Destroy(mCcContext->cc_handle);
    }
    LOGI("start cc failed!");
    return -1;
}

int ClosedCaptionParser::stopAmlCC() {
    LOGI("stop cc");
    AM_CC_Destroy(mCcContext->cc_handle);
    pthread_mutex_lock(&mCcContext->lock);
    pthread_mutex_unlock(&mCcContext->lock);

    mCcContext->cc_handle = NULL;
    gHandle = {true, 0};
    return 0;
}



int ClosedCaptionParser::startAmlCC() {
    int source = mChannelId>>8;
    int channel = mChannelId&0xff;
    if (source != AM_CC_INPUT_VBI) {
        mVfmt = VideoInfo::Instance()->getVideoFormat();
    }

    if (sInstance == nullptr) {
        sInstance = this;
    }

    LOGI(" start cc source:%d, channel:%d, mvfmt:%d", source, channel, mVfmt);
    startAtscCc(source, mVfmt, channel, 0, 0, 0, 0, 0, 0);

    return 0;
}


int ClosedCaptionParser::parse() {
    // CC use little dvb, no need common parser here
    if (!mThreadExitRequested) {
        if (mState == SUB_INIT) {
            /*this have multi thread issue.
            startAmlCC have delay, if this mState after startAmlCC,
            then resumeplay will cause stopParser first, mState will SUB_STOP to
            SUB_PLAYING, which cause while circle, and cc thread will not release*/
            mState = SUB_PLAYING;

            startAmlCC();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return 0;
    }
    return 0;
}

void ClosedCaptionParser::dump(int fd, const char *prefix) {
    dprintf(fd, "%s Closed Caption Parser\n", prefix);
    dumpCommon(fd, prefix);
    dprintf(fd, "\n");
    dprintf(fd, "%s   Channel Id: %d\n", prefix, mChannelId);
    dprintf(fd, "%s   Device  No: %d\n", prefix, mDevNo);
    dprintf(fd, "%s   Video Format: %d\n", prefix, mVfmt);
    dprintf(fd, "\n");
    if (mCcContext != nullptr) {
        mCcContext->dump(fd, prefix);
    }
}



