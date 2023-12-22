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

#define LOG_TAG "Subtitle"

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <chrono>
#include <thread>

#include <utils/CallStack.h>
#include "SubtitleLog.h"

#include "Subtitle.h"
#include "Parser.h"
#include "ParserFactory.h"


Subtitle::Subtitle() :
    mSubtitleTracks(-1),
    mCurrentSubtitleType(-1),
    mRenderTime(-1),
    mParserNotifier(nullptr),
    mExitRequested(false),
    mThread(nullptr),
    mPendingAction(-1),
    mIsExtSub(false),
    mIdxSubTrack(-1)
 {
    SUBTITLE_LOGI("%s", __func__);
}

Subtitle::Subtitle(bool isExtSub, int trackId, ParserEventNotifier *notifier) :
        mSubtitleTracks(-1),
        mCurrentSubtitleType(-1),
        mRenderTime(-1),
        mExitRequested(false),
        mThread(nullptr),
        mPendingAction(-1)
{
    SUBTITLE_LOGI("%s", __func__);
    mParserNotifier = notifier;
    mIsExtSub = isExtSub;
    mIdxSubTrack = trackId;
}

void Subtitle::init(int renderType) {
    mSubPrams = std::shared_ptr<SubtitleParamType>(new SubtitleParamType());
    mSubPrams->dvbParam.demuxId = -1;
    mSubPrams->dvbParam.pid= -1;
    mSubPrams->dvbParam.ancillaryId = -1;
    mSubPrams->dvbParam.compositionId= -1;

    mSubPrams->arib24Param.demuxId = -1;
    mSubPrams->arib24Param.pid= -1;
    mSubPrams->arib24Param.languageCodeId = -1;
    mSubPrams->ttmlParam.demuxId = -1;
    mSubPrams->ttmlParam.pid= -1;
    mSubPrams->smpteTtmlParam.demuxId = 0;
    mSubPrams->smpteTtmlParam.pid= 0;
    mPresentation = std::shared_ptr<Presentation>(new Presentation(nullptr, renderType));
}


Subtitle::~Subtitle() {
    SUBTITLE_LOGI("%s", __func__);
    //android::CallStack(LOG_TAG);
    mExitRequested = true;
    mCv.notify_all();
    if (mThread != nullptr) {
        mThread->join();
    }

    if (mDataSource != nullptr) {
        mDataSource->stop();
    }

    if (mParser != nullptr) {
        mParser->stopParser();
        mPresentation->stopPresent();
        mParser = nullptr;
    }

    SUBTITLE_LOGI("%s end", __func__);
}

void Subtitle::attachDataSource(std::shared_ptr<DataSource> source, std::shared_ptr<InfoChangeListener>listener) {
     SUBTITLE_LOGI("%s", __func__);
    mDataSource = source;
    mDataSource->registerInfoListener(listener);
    mDataSource->start();
}

void Subtitle::dettachDataSource(std::shared_ptr<InfoChangeListener>listener) {
    //mRenderTime = renderTime;
    if (mDataSource != nullptr) {
        mDataSource->unregisteredInfoListener(listener);
    }
}


void Subtitle::onSubtitleChanged(int newTotal) {
    if (newTotal == mSubtitleTracks) return;
    SUBTITLE_LOGI("onSubtitleChanged:%d", newTotal);
    mSubtitleTracks = newTotal;
}

void Subtitle::onRenderStartTimestamp(int64_t startTime) {
    //mRenderTime = renderTime;
    if (mPresentation != nullptr) {
        mPresentation->notifyStartTimeStamp(startTime);
    }
}

void Subtitle::onRenderTimeChanged(int64_t renderTime) {
    //SUBTITLE_LOGI("onRenderTimeChanged:%lld", renderTime);
    mRenderTime = renderTime;
    if (mPresentation != nullptr) {
        mPresentation->syncCurrentPresentTime(mRenderTime);
    }
}

void Subtitle::onGetLanguage(std::string &lang) {
    mCurrentLanguage = lang;
}

void Subtitle::onTypeChanged(int newType) {

    std::unique_lock<std::mutex> autolock(mMutex);
    if (newType == mCurrentSubtitleType) return;

    SUBTITLE_LOGI("onTypeChanged:%d", newType);
    if (newType <= TYPE_SUBTITLE_INVALID || newType >= TYPE_SUBTITLE_MAX) {
        SUBTITLE_LOGI("Error! invalid type!%d", newType);
        return;
    }
    mCurrentSubtitleType = newType;
    mSubPrams->subType = static_cast<SubtitleType>(newType);

    // need handle
    mPendingAction = ACTION_SUBTITLE_RECEIVED_SUBTYPE;
    mCv.notify_all(); // let handle it
}

int Subtitle::onMediaCurrentPresentationTime(int ptsMills) {
    unsigned int pts = (unsigned int)ptsMills;

    if (mPresentation != nullptr) {
        mPresentation->syncCurrentPresentTime(pts);
    }
    return 0;
}

int Subtitle::getTotalSubtitles() {
    return mSubtitleTracks;
}


int Subtitle::getSubtitleType() {
    return mCurrentSubtitleType;
}
std::string Subtitle::currentLanguage() {
    return mCurrentLanguage;
}

/* currently, we  */
bool Subtitle::setParameter(void *params) {
    std::unique_lock<std::mutex> autolock(mMutex);
    SubtitleParamType *p = new SubtitleParamType();
    *p = *(static_cast<SubtitleParamType*>(params));

    //android::CallStack here("here");

    mSubPrams = std::shared_ptr<SubtitleParamType>(p);
    mSubPrams->update();// need update before use

    //process ttx skip page func.
    if ((mSubPrams->subType == TYPE_SUBTITLE_DVB) || (mSubPrams->subType == TYPE_SUBTITLE_DVB_TELETEXT) || (mSubPrams->subType == TYPE_SUBTITLE_DVB_TTML)
        || (mSubPrams->subType == TYPE_SUBTITLE_CLOSED_CAPTION) || (mSubPrams->subType == TYPE_SUBTITLE_SCTE27) || (mSubPrams->subType == TYPE_SUBTITLE_ARIB_B24)
        || (mSubPrams->subType == TYPE_SUBTITLE_SMPTE_TTML)) {
        mPendingAction = ACTION_SUBTITLE_SET_PARAM;
        mCv.notify_all();
        return true;
    }
    mPendingAction = ACTION_SUBTITLE_RECEIVED_SUBTYPE;
    mCv.notify_all();
    return true;
}

bool Subtitle::resetForSeek() {
    mPendingAction = ACTION_SUBTITLE_RESET_FOR_SEEK;
    mCv.notify_all();
    return true;
}

// TODO: actually, not used now
void Subtitle::scheduleStart() {
    SUBTITLE_LOGI("scheduleStart:%d", mSubPrams->subType);
    if (nullptr != mDataSource) {
        mDataSource->start();
    }
    mThread = std::shared_ptr<std::thread>(new std::thread(&Subtitle::run, this));
}

// Run in a new thread. any access to this object's member field need protected by lock
void Subtitle::run() {
    // check exit
    bool ret = false;
    SUBTITLE_LOGI("run mExitRequested:%d, mSubPrams->subType:%d", mExitRequested, mSubPrams->subType);

    while (!mExitRequested) {
        std::unique_lock<std::mutex> autolock(mMutex);
        mCv.wait_for(autolock, std::chrono::milliseconds(100));

        if (mIsExtSub && mParser == nullptr) {
            mSubPrams->subType = TYPE_SUBTITLE_EXTERNAL;// if mFd > 0 is Ext sub
            mSubPrams->idxSubTrackId = mIdxSubTrack;
            mParser = ParserFactory::create(mSubPrams, mDataSource);
            if (mParser == nullptr) {
                SUBTITLE_LOGE("Parser creat failed, break!");
                break;
            } else {
                mParser->startParser(mParserNotifier, mPresentation.get());
            }
            if (mPresentation != nullptr) {
                ret = mPresentation->startPresent(mParser);
            }
            mPendingAction = -1; // No need handle
        }else if(mIsExtSub && mParser->getParseType() == TYPE_SUBTITLE_CLOSED_CAPTION){
            /*
             * When ccparser is created by default,
             * it can be changed to the correct parser type
             * when specifying the parser type subsequently
             */
            mParser->stopParser();
            mPresentation->stopPresent();
            mParser = nullptr;
            mSubPrams->subType = TYPE_SUBTITLE_EXTERNAL;// if mFd > 0 is Ext sub
            mSubPrams->idxSubTrackId = mIdxSubTrack;
            mParser = ParserFactory::create(mSubPrams, mDataSource);
            if (mParser == nullptr) {
                SUBTITLE_LOGE("Parser creat failed, break!");
                break;
            }
            mParser->startParser(mParserNotifier, mPresentation.get());
            mPresentation->startPresent(mParser);
            mPendingAction = -1; // No need handle
        }

        switch (mPendingAction) {
            case ACTION_SUBTITLE_SET_PARAM: {
                bool createAndStart = false;
                if (mParser == nullptr) {
                    // when the first time start subtitle, some parameter may affect the behavior
                    // such as cached teletext. we use tsid/onid/pid to check need use cached ttx or not
                    createAndStart = true;
                    mParser = ParserFactory::create(mSubPrams, mDataSource);
                }
                SUBTITLE_LOGI("run ACTION_SUBTITLE_SET_PARAM %d %d", mSubPrams->subType, TYPE_SUBTITLE_CLOSED_CAPTION);
                if (mParser != nullptr) {
                    if (mSubPrams->subType == TYPE_SUBTITLE_DVB) {
                        mParser->updateParameter(TYPE_SUBTITLE_DVB, &mSubPrams->dvbParam);
                    } else if (mSubPrams->subType == TYPE_SUBTITLE_DVB_TELETEXT) {
                        mParser->updateParameter(TYPE_SUBTITLE_DVB_TELETEXT, &mSubPrams->teletextParam);
                    } else if (mSubPrams->subType == TYPE_SUBTITLE_DVB_TTML) {
                        mParser->updateParameter(TYPE_SUBTITLE_DVB_TTML, &mSubPrams->ttmlParam);
                    } else if (mSubPrams->subType == TYPE_SUBTITLE_ARIB_B24) {
                        mParser->updateParameter(TYPE_SUBTITLE_ARIB_B24, &mSubPrams->arib24Param);
                    } else if (mSubPrams->subType == TYPE_SUBTITLE_SCTE27) {
                        mParser->updateParameter(TYPE_SUBTITLE_SCTE27, &mSubPrams->scte27Param);
                    } else if (mSubPrams->subType == TYPE_SUBTITLE_CLOSED_CAPTION) {
                        mParser->updateParameter(TYPE_SUBTITLE_CLOSED_CAPTION, &mSubPrams->closedCaptionParam);
                    } else if (mSubPrams->subType == TYPE_SUBTITLE_SMPTE_TTML) {
                        mParser->updateParameter(TYPE_SUBTITLE_SMPTE_TTML, &mSubPrams->smpteTtmlParam);
                    }

                    if (createAndStart) {
                      if (mParser != nullptr) {
                          mParser->startParser(mParserNotifier, mPresentation.get());
                      }
                      if (mPresentation != nullptr) {
                          ret = mPresentation->startPresent(mParser);
                      }
                    }
                }
            }
            break;
            case ACTION_SUBTITLE_RECEIVED_SUBTYPE: {
                SUBTITLE_LOGI("ACTION_SUBTITLE_RECEIVED_SUBTYPE, type:%d", mSubPrams->subType);
                if (mParser != nullptr) {
                    mParser->stopParser();
                    mPresentation->stopPresent();
                    mParser = nullptr;
                }
                mParser = ParserFactory::create(mSubPrams, mDataSource);
                if (mParser != nullptr) {
                    mParser->startParser(mParserNotifier, mPresentation.get());
                }
                if (mPresentation != nullptr) {
                    ret = mPresentation->startPresent(mParser);
                }

                if (mParser != nullptr) {
                    if (mSubPrams->subType == TYPE_SUBTITLE_DVB) {
                        mParser->updateParameter(TYPE_SUBTITLE_DVB, &mSubPrams->dvbParam);
                    } else if (mSubPrams->subType == TYPE_SUBTITLE_DVB_TELETEXT) {
                        mParser->updateParameter(TYPE_SUBTITLE_DVB_TELETEXT, &mSubPrams->teletextParam);
                    } else if (mSubPrams->subType == TYPE_SUBTITLE_DVB_TTML) {
                        mParser->updateParameter(TYPE_SUBTITLE_DVB_TTML, &mSubPrams->ttmlParam);
                    } else if (mSubPrams->subType == TYPE_SUBTITLE_CLOSED_CAPTION) {
                        mParser->updateParameter(TYPE_SUBTITLE_CLOSED_CAPTION, &mSubPrams->closedCaptionParam);
                    } else if (mSubPrams->subType == TYPE_SUBTITLE_SCTE27) {
                        mParser->updateParameter(TYPE_SUBTITLE_SCTE27, &mSubPrams->scte27Param);
                    } else if (mSubPrams->subType == TYPE_SUBTITLE_ARIB_B24) {
                        mParser->updateParameter(TYPE_SUBTITLE_ARIB_B24, &mSubPrams->arib24Param);
                    } else if (mSubPrams->subType == TYPE_SUBTITLE_SMPTE_TTML) {
                        mParser->updateParameter(TYPE_SUBTITLE_SMPTE_TTML, &mSubPrams->smpteTtmlParam);
                    }
                }
            }
            break;
            case ACTION_SUBTITLE_RESET_MEDIASYNC:
                if (mParser != nullptr) {
                    mParser->setPipId(PIP_MEDIASYNC_ID, mSubPrams->mediaId);
                }
            break;
            case ACTION_SUBTITLE_RESET_FOR_SEEK:
                if (mParser != nullptr) {
                    mParser->resetForSeek();
                }

                if (mPresentation != nullptr) {
                    mPresentation->resetForSeek();
                }
            break;
        }
        // handled
        mPendingAction = -1;

        // wait100ms, still no parser, then start default CC
        if (mSubPrams->playerId <= 0) {
            SUBTITLE_LOGI("%s, playerid is an invalid value, playerid = %d",__func__, mSubPrams->playerId);
        }

        if (mParser == nullptr) {
            int value = (mSubPrams->closedCaptionParam.ChannelID >> 8) == 1? 1:(mSubPrams->playerId > 0 ? 1:0);
            if (value) {
                SUBTITLE_LOGI("No parser found, create default!");
                // start default parser, normally, this is CC
                mParser = ParserFactory::create(mSubPrams, mDataSource);
                if (mParser == nullptr) {
                    SUBTITLE_LOGE("Parser creat failed, break!");
                    break;
                } else {
                    mParser->startParser(mParserNotifier, mPresentation.get());
                }

                if (mPresentation != nullptr) {
                    ret = mPresentation->startPresent(mParser);
                }
            }
        }

    }

    SUBTITLE_LOGI("Exit: run mExitRequested:%d, mSubPrams->subType:%d", mExitRequested, mSubPrams->subType);
}

void Subtitle::dump(int fd) {
    dprintf(fd, "\n\n Subtitle:\n");
    dprintf(fd, "--------------------------------------------------------------------------------------\n");
    dprintf(fd, "isExitedRequested? %s\n", mExitRequested?"Yes":"No");
    dprintf(fd, "PendingAction: %d\n", mPendingAction);
    dprintf(fd, "\n");
    dprintf(fd, "DataSource  : %p\n", mDataSource.get());
    dprintf(fd, "Presentation: %p\n", mPresentation.get());
    dprintf(fd, "Parser      : %p\n", mParser.get());
    dprintf(fd, "\n");
    dprintf(fd, "Total Subtitle Tracks: %d\n", mSubtitleTracks);
    dprintf(fd, "Current Subtitle type: %d\n", mCurrentSubtitleType);
    dprintf(fd, "Current Video PTS    : %lld\n", mRenderTime);

    if (mSubPrams != nullptr) {
        dprintf(fd, "\nSubParams:\n");
        mSubPrams->dump(fd, "  ");
    }

    if (mPresentation != nullptr) {
        dprintf(fd, "\nPresentation: %p\n", mPresentation.get());
        mPresentation->dump(fd, "   ");
    }

   // std::shared_ptr<Parser> mParser;
    if (mParser != nullptr) {
        dprintf(fd, "\nParser: %p\n", mParser.get());
        mParser->dump(fd, "   ");
    }


}

