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


#define LOG_TAG "SubSocketServer"

#include <fcntl.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/poll.h>
#include "SubtitleLog.h"

#include "DataSource.h"
#include "Segment.h"

#include "SocketServer.h"
#include "EventsTracker.h"
#include "IpcBuffer.h"
#include "../SubtitleServer.h"
#define USE_UN true
#if USE_UN
#define UN_BASE_PATH "/tmp/subtitlesocketserver.sock"
#endif

/**
    payload is:
        startFlag   : 4bytes
        sessionID   : 4Bytes (TBD)
        magic       : 4bytes (for double confirm)
        payload Type: defined in PayloadType_t
        payload size: 4Bytes (indicate the size of payload, size is total_send_bytes - 4*5)
                      exclude this and above 4 items.
        payload data: TO BE PARSED data
*/

/**
    payload is:
        startFlag   : 4bytes
        sessionID   : 4Bytes (TBD)
        magic       : 4bytes (for double confirm)
        payload Type: defined in PayloadType_t
        payload size: 4Bytes (indicate the size of payload, size is total_send_bytes - 4*5)
                      exclude this and above 4 items.
        payload data: TO BE PARSED data
*/

static inline void dump(const char *buf, int size) {
    char str[64] = {0};

    for (int i = 0; i < size; i++) {
        char chars[6] = {0};
        sprintf(chars, "%02x ", buf[i]);
        strcat(str, chars);
        if (i % 8 == 7) {
            SUBTITLE_LOGI("%s", str);
            str[0] = str[1] = 0;
        }
    }
    SUBTITLE_LOGI("%s", str);
}


static std::mutex _g_inst_mutex;

SubSocketServer::SubSocketServer() : mExitRequested(false) {
    SUBTITLE_LOGI("%s ?", __func__);

    mEventsTracker = std::make_shared<EventsTracker>(SubSocketServer::handleEvent);
}

SubSocketServer::~SubSocketServer() {
    SUBTITLE_LOGI("%s", __func__);

    if (mEventsTracker != nullptr) {
        mEventsTracker->requestExit();
    }

    mExitRequested = true;
    mDispatchThread.join();
}

SubSocketServer *SubSocketServer::mInstance = nullptr;

SubSocketServer *SubSocketServer::GetInstance() {
    SUBTITLE_LOGI("%s", __func__);

    std::unique_lock<std::mutex> autolock(_g_inst_mutex);
    if (mInstance == nullptr) {
        mInstance = new SubSocketServer();
    }

    return mInstance;
}

int SubSocketServer::serve() {
    std::thread t = std::thread(&SubSocketServer::__threadLoop, this);
    t.detach();
    return 0;
}

void SubSocketServer::__threadLoop() {
    while (!mExitRequested && threadLoop());
    SUBTITLE_LOGI("%s: EXIT!!!", __FUNCTION__);
}

bool SubSocketServer::threadLoop() {
    SUBTITLE_LOGI("%s", __FUNCTION__);
    int flag = 1;
    int sockFd = socket(USE_UN ? AF_UNIX : AF_INET, SOCK_STREAM, 0);
    if (sockFd < 0) return true;

    if ((setsockopt(sockFd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag))) < 0) {
        SUBTITLE_LOGE("setsockopt failed.\n");
        close(sockFd);
        return true;
    }

#if USE_UN
    std::string unPath = std::string(UN_BASE_PATH);

    struct sockaddr_un un{};
    bzero(&un, sizeof(un));
    un.sun_family = AF_UNIX;
    strcpy(un.sun_path, unPath.c_str());

    unlink(unPath.c_str());
    size_t sockLen = offsetof(struct sockaddr_un, sun_path) + unPath.size();

    if (::bind(sockFd, (struct sockaddr *) &un, (socklen_t) sockLen) == -1) {
        SUBTITLE_LOGE("bind as UN fail. error=%d, err:%s\n", errno, strerror(errno));
        close(sockFd);
        return false;
    }
#else
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (::bind(sockFd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        SUBTITLE_LOGE("bind as INET fail. error=%d, err:%s\n", errno, strerror(errno));
        close(sockFd);
        return false;
    }
#endif

    if (::listen(sockFd, QUEUE_SIZE) == -1) {
        SUBTITLE_LOGE("listen fail.error=%d, err:%s\n", errno, strerror(errno));
        close(sockFd);
        return true;
    }

    mIsServing = true;

    SUBTITLE_LOGI("[startServerThread] listen success.\n");
    while (!mExitRequested) {
        struct sockaddr_in client_addr;
        socklen_t length = sizeof(client_addr);
        int connFd = accept(sockFd, (struct sockaddr *) &client_addr, &length);
        if (connFd < 0) {
            close(sockFd);
            return true;
        }

        SUBTITLE_LOGI("New connection coming: fd=%d", connFd);
        auto *dataObj = new DataObj_t;
        dataObj->obj1 = this;
        mEventsTracker->addFd(connFd,
                              EventsTracker::EVENT_INPUT | EventsTracker::EVENT_ERROR, dataObj);
    }

    SUBTITLE_LOGI("closed.\n");
    close(sockFd);
    mEventsTracker->requestExit();
    mIsServing = false;
    return true;
}

int SubSocketServer::handleEvent(int fd, int events, void *data) {
    auto *dataObj = static_cast<DataObj_t *>(data);
    if (dataObj == nullptr) {
        SUBTITLE_LOGE("data DataObj_t == null.");
        close(fd);
        return EventsTracker::RET_REMOVE;
    }

    auto subSocketServer = static_cast<SubSocketServer *>(dataObj->obj1);
    if (subSocketServer == nullptr) {
        SUBTITLE_LOGE("data SubSocketServer == null.");
        close(fd);
        ringbuffer_free(dataObj->obj2);
        return EventsTracker::RET_REMOVE;
    }

//    EventsTracker *tracker = subSocketServer->mEventsTracker.get();
//    if (tracker == nullptr) {
//        /*Do not listen anymore*/
//        SUBTITLE_LOGE("data EventsTracker == null.");
//        close(fd);
//        ringbuffer_free(dataObj->obj2);
//        return EventsTracker::RET_REMOVE;
//    }

    if (events & EventsTracker::EVENT_INPUT) {
        SUBTITLE_LOGI("%s: fd= %d, events= EVENT_INPUT", __FUNCTION__, fd);

        int ret = subSocketServer->processData(fd, dataObj);
        if (ret == EventsTracker::RET_REMOVE) {
            if (dataObj->obj2 != nullptr) {
                delete (IpcBuffer *) dataObj->obj2;
            }
            dataObj->reset();
            close(fd);
        }
        return ret;

    } else if (events & EventsTracker::EVENT_ERROR) {
        SUBTITLE_LOGE("Error occurred for fd %d, remove.", fd);
        if (dataObj->obj2 != nullptr) {
            delete (IpcBuffer *) dataObj->obj2;
        }

        dataObj->reset();
        close(fd);
        return EventsTracker::RET_REMOVE;
    }

    return EventsTracker::RET_CONTINUE;
}

int SubSocketServer::processData(int fd, DataObj_t *dataObj) {
    //1. Precondition for data
    if (dataObj->obj2 == nullptr) {
        SUBTITLE_LOGI("create new handle for fd %d", fd);
        dataObj->obj2 = new IpcBuffer(512 * 1024, "socket_buffer");
    } else {
        SUBTITLE_LOGI("handle already created for fd %d", fd);
    }

    auto ipcBuffer = static_cast<IpcBuffer *>(dataObj->obj2);

    if (ipcBuffer->_data_header == nullptr) {
        ipcBuffer->_data_header = new IpcPackageHeader;
    }

    char recvBuf[1024] = {0};
    int retLen = ::recv(fd, recvBuf, sizeof(recvBuf), 0);
    if (retLen <= 0) {
        SUBTITLE_LOGI("Client broken, fd= %d, remove.", fd);
        auto subSocketServer = static_cast<SubSocketServer *>(dataObj->obj1);
        int sessionId = ipcBuffer->_data_header->sessionId;
        subSocketServer->onRemoteDead(sessionId < 0 ? 0 : sessionId);
        return EventsTracker::RET_REMOVE;
    }

    if (mClients.empty()) {
        SUBTITLE_LOGE("mClients is empty, do not process now.");
        return EventsTracker::RET_CONTINUE;
    }

    //2. Process data
    ipcBuffer->write(recvBuf, retLen);
    if (ipcBuffer->readableCount() < sizeof(IpcPackageHeader)) {
        SUBTITLE_LOGE("No enough data for Header.");
        return EventsTracker::RET_CONTINUE;
    }

    // Check sync word.
    int sizeOfInt = sizeof(int);
    char buffer[sizeof(IpcPackageHeader)];
    ipcBuffer->peek(buffer, sizeOfInt);
    int syncWord = peekAsSocketWord(buffer);
    if (syncWord == START_FLAG) {
        ipcBuffer->read(buffer, sizeOfInt);
        ipcBuffer->read(buffer + sizeOfInt, sizeof(IpcPackageHeader) - sizeOfInt);
        ipcBuffer->_data_header->syncWord = syncWord;
        ipcBuffer->_data_header->sessionId = peekAsSocketWord(buffer + sizeOfInt);
        ipcBuffer->_data_header->magicWord = peekAsSocketWord(buffer + sizeOfInt * 2);
        ipcBuffer->_data_header->dataSize = peekAsSocketWord(buffer + sizeOfInt * 3);
        ipcBuffer->_data_header->pkgType = peekAsSocketWord(buffer + sizeOfInt * 4);

        SUBTITLE_LOGI("Find header:\n"
              "struct IpcPackageHeader {\n"
              "    int syncWord = %d;\n"
              "    int sessionId = %d;\n"
              "    int magicWord = %d;\n"
              "    int dataSize = %d;\n"
              "    int pkgType = %d;\n"
              "};",
              ipcBuffer->_data_header->syncWord,
              ipcBuffer->_data_header->sessionId,
              ipcBuffer->_data_header->magicWord,
              ipcBuffer->_data_header->dataSize,
              ipcBuffer->_data_header->pkgType);

        if (eTypeSubtitleExitServ == ipcBuffer->_data_header->pkgType
            || 'XEDC' == ipcBuffer->_data_header->pkgType) {
            SUBTITLE_LOGI("exit requested!");
            return EventsTracker::RET_REMOVE;
        }
    }

    if (ipcBuffer->readableCount() < ipcBuffer->_data_header->dataSize) {
        //Maybe data is coming.
        SUBTITLE_LOGI("Buffer readableCount(%d) < Needed data size(%d), waiting more data.",
                ipcBuffer->readableCount(), ipcBuffer->_data_header->dataSize);
        return EventsTracker::RET_CONTINUE;
    }

    char *payloads = (char *) malloc(ipcBuffer->_data_header->dataSize + sizeOfInt);
    memcpy(payloads, &ipcBuffer->_data_header->pkgType, sizeOfInt); // fill package type
    ipcBuffer->read(payloads + sizeOfInt, ipcBuffer->_data_header->dataSize);
    {// notify listener
        std::lock_guard<std::mutex> guard(mLock);
        DataListener *listener = mClients.front();
        if (listener != nullptr) {
            if (listener->onData(payloads, ipcBuffer->_data_header->dataSize + sizeOfInt) < 0) {
                // if returen -1, means client exit, this request clientConnected exit!
                free(payloads);
                return EventsTracker::RET_REMOVE;
            }
        }
    }
    free(payloads);
    ipcBuffer->reset();

    return EventsTracker::RET_CONTINUE;
}

bool SubSocketServer::registClient(DataListener *client) {
    std::lock_guard<std::mutex> guard(GetInstance()->mLock);
    GetInstance()->mClients.push_back(client);
    SUBTITLE_LOGI("registClient: %p size=%d", client, GetInstance()->mClients.size());
    return true;
}

bool SubSocketServer::unregisterClient(DataListener *client) {
    // obviously, BUG here! impl later, support multi-client.
    // TODO: revise the whole mClient, if we want to support multi subtitle

    std::lock_guard<std::mutex> guard(GetInstance()->mLock);

    std::vector<DataListener *> &vecs = GetInstance()->mClients;
    if (vecs.size() > 0) {
        for (auto it = vecs.cbegin(); it != vecs.cend(); it++) {
            if ((*it) == client) {
                vecs.erase(it);
                break;
            }
        }

        //GetInstance()->mClients.pop_back();
        SUBTITLE_LOGI("unregisterClient: %p size=%d", client, GetInstance()->mClients.size());
    }
    return true;
}

void SubSocketServer::onRemoteDead(int sessionId) {
    SUBTITLE_LOGI("onRemoteDead, sessionId= %d", sessionId);

    SubtitleServer *server = SubtitleServer::Instance();
    if (server && !server->isClosed(sessionId)) {
        if (server->close(sessionId) == Result::OK
               && server->closeConnection(sessionId) == Result::OK) {
            SUBTITLE_LOGI("onRemoteDead, self close OK for id %d", sessionId);
        } else {
            SUBTITLE_LOGI("onRemoteDead, self close failed for id %d", sessionId);
        }
    } else {
        SUBTITLE_LOGI("onRemoteDead, already closed for id %d", sessionId);
    }
}

