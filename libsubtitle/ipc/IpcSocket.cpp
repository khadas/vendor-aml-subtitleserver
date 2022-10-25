/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#define LOG_TAG "IpcSocket"

#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <IpcSocket.h>
#include <utils/Log.h>

#include <cstring>
#include <memory>
#include <mutex>
#include <zconf.h>
#include <errno.h>

#include <BlockChannel.h>

#define SEND_MAX_LEN 1024

// For local socket
#define USE_UN true
#if USE_UN
#define UN_BASE_PATH "/tmp/subtitle.sock."
#endif

const int eventInputOnly = EventsTracker::EVENT_INPUT | EventsTracker::EVENT_ERROR;
static std::mutex mMutex;

static double now(int clockId = CLOCK_MONOTONIC) {
    struct timespec spec;
    clock_gettime(clockId, &spec);
    return spec.tv_sec;
}

IpcSocket::IpcSocket(bool isServer)
        : mIsServer(isServer),
          mIsValid(false),
          mListenFd(-1) {

    ALOGD("%s +++ (%s)", __FUNCTION__, (mIsServer ? "Server" : "Client"));
}

IpcSocket::~IpcSocket() {
    ALOGD("%s --- (%s)", __FUNCTION__, (mIsServer ? "Server" : "Client"));
    mSelfData = nullptr;
    close(mListenFd);
    mListenFd = -1;
}

void IpcSocket::start(int port, void *selfData, OnThreadStart onThreadStart) {
    mEventsTracker = std::make_shared<EventsTracker>(IpcSocket::handleEvents, "IpcSocket",
                                                     onThreadStart);

    mSelfData = selfData;

    if (mIsServer) {
        mIsValid = initAsServer(port);
    } else {
        mIsValid = initAsClient(port);
    }

    ALOGD("Init %s(%d) %s",
          (mIsServer ? "Server" : "Client"),
          port,
          (mIsValid ? "success" : "failed"));
}

bool IpcSocket::isvalid() {
    return mIsValid;
}

bool IpcSocket::initAsServer(int port) {
    int flag = 1;
    int sockFd = socket(USE_UN ? AF_UNIX : AF_INET, SOCK_STREAM, 0);
    if (sockFd < 0) return false;

    if ((setsockopt(sockFd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag))) < 0) {
        ALOGE("setsockopt failed.\n");
        close(sockFd);
        return false;
    }

#if USE_UN
    std::string unPath = std::string(UN_BASE_PATH).append(std::to_string(port));

    struct sockaddr_un un{};
    bzero(&un, sizeof(un));
    un.sun_family = AF_UNIX;
    strcpy(un.sun_path, unPath.c_str());

    unlink(unPath.c_str());
    size_t sockLen = offsetof(struct sockaddr_un, sun_path) + unPath.size();

    if (::bind(sockFd, (struct sockaddr *) &un, (socklen_t) sockLen) == -1) {
        ALOGE("bind as UN fail. error=%d, err:%s\n", errno, strerror(errno));
        close(sockFd);
        return false;
    }
#else
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (::bind(sockFd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        ALOGE("bind as INET fail. error=%d, err:%s\n", errno, strerror(errno));
        close(sockFd);
        return false;
    }
#endif

    if (::listen(sockFd, 10) == -1) {
        ALOGE("listen fail.error=%d, err:%s\n", errno, strerror(errno));
        close(sockFd);
        return false;
    }

    mListenFd = sockFd;

    DataObj_t *dataObj = new DataObj_t;
    dataObj->obj1 = this;
    dataObj->obj2 = mSelfData;
    mEventsTracker->addFd(mListenFd, eventInputOnly, dataObj);
    return true;
}

bool IpcSocket::initAsClient(int port) {
#if USE_UN
    mListenFd = socket(AF_UNIX, SOCK_STREAM, 0);
    std::string unPath = std::string(UN_BASE_PATH).append(std::to_string(port));

    struct sockaddr_un un{};
    bzero(&un, sizeof(un));
    un.sun_family = AF_UNIX;
    strcpy(un.sun_path, unPath.c_str());

    size_t sockLen = offsetof(struct sockaddr_un, sun_path) + unPath.size();

    int64_t startTime = now();
    while (true) {
        if (::connect(mListenFd, (struct sockaddr *) &un, (socklen_t) sockLen) < 0) {
            //load dvb so need time since add close caption subtitle, add to 120ms.
            //only has subtitle to connect socket
            if (now() - startTime > 120000ll) {
                ALOGE("%s:%d, connect socket failed!, error=%d, err:%s\n", __FILE__, __LINE__,
                      errno, strerror(errno));
                close(mListenFd);
                mListenFd = -1;
                return false;
            }
        } else {
            break;
        }
    }
#else
    mListenFd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr{};
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (mListenFd < 0) {
        ALOGE("%s:%d, create socket failed!mSockFd:%d, error=%d, err:%s\n", __FILE__, __LINE__,
              mListenFd, errno, strerror(errno));
        return false;
    }

    int64_t startTime = now();
    while (true) {
        if (::connect(mListenFd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
            //load dvb so need time since add close caption subtitle, add to 120ms.
            //only has subtitle to connect socket
            if (now() - startTime > 120000ll) {
                ALOGE("%s:%d, connect socket failed!, error=%d, err:%s\n", __FILE__, __LINE__,
                      errno, strerror(errno));
                close(mListenFd);
                mListenFd = -1;
                return false;
            }
        } else {
            break;
        }
    }
#endif

    ALOGI("%s:%d, connect socket success!mSockFd:%d\n", __FILE__, __LINE__, mListenFd);

    DataObj_t *dataObj = new DataObj_t;
    dataObj->obj1 = this;
    dataObj->obj2 = mSelfData;
    mEventsTracker->addFd(mListenFd, eventInputOnly, dataObj);

    return true;
}

int IpcSocket::handleEvents(int fd, int events, void *data) {
    auto *dataObj = static_cast<DataObj_t *>(data);
    if (dataObj == nullptr) {
        ALOGE("data DataObj_t == null.");
        close(fd);
        return EventsTracker::RET_REMOVE;
    }

    auto ipcSocket = static_cast<IpcSocket *>(dataObj->obj1);
    if (ipcSocket == nullptr) {
        close(fd);
        dataObj->reset();
        delete dataObj;
        dataObj = nullptr;
        return EventsTracker::RET_REMOVE;
    }

    if (events & EventsTracker::EVENT_ERROR) {
        ALOGE("[%s] Error for %s fd(%d)", __FUNCTION__,
              (ipcSocket->mIsServer ? "Server" : "Client"), fd);
        close(fd);
        dataObj->reset();
        delete dataObj;
        dataObj = nullptr;
        return EventsTracker::RET_REMOVE;
    }

    ALOGD("handleEvents: %s", (ipcSocket->mIsServer ? "Server" : "Client"));

    int ret;
    if (ipcSocket->mIsServer) {
        ret = ipcSocket->handleServerEvents(fd, events, data);
    } else {
        ret = ipcSocket->handleClientEvents(fd, events, dataObj->obj2);
    }

    if (ret == EventsTracker::RET_REMOVE) {
        close(fd);
        dataObj->reset();
        delete dataObj;
    }

    return ret;
}

bool IpcSocket::processListenFd(int fd) {
    int connFd;
#if USE_UN
    struct sockaddr_un un{};
    socklen_t length = sizeof(un);
    connFd = accept(fd, (struct sockaddr *) &un, &length);
    if (connFd < 0) {
        ALOGE("[%s], accept fd(%d) failed", __FUNCTION__, fd);
        close(fd);
    }
#else
        struct sockaddr_in client_addr{};
        socklen_t length = sizeof(client_addr);
        connFd = accept(fd, (struct sockaddr *) &client_addr, &length);
        if (connFd < 0) {
            ALOGE("[%s], accept fd(%d) failed", __FUNCTION__, fd);
            close(fd);
            return false;
        }
#endif

    ALOGD("New connection comming: fd=%d", connFd);
    auto *dataObj = new DataObj_t;
    dataObj->obj1 = this;
    dataObj->obj2 = mSelfData;
    mEventsTracker->addFd(connFd, eventInputOnly, dataObj);
    return true;
}

int IpcSocket::processFdFromClient(int fd, int events, void *data) {
    if (mEventReceivedListenerFunc != nullptr) {
        if (events & EventsTracker::EVENT_INPUT) {
            return mEventReceivedListenerFunc(fd, data);
        } else {
            return EventsTracker::RET_CONTINUE;
        }
    }

    //No need for don't receive events' fd.
    return EventsTracker::RET_REMOVE;
}

int IpcSocket::handleClientEvents(int fd, int events, void *data) {
    if (mEventReceivedListenerFunc != nullptr) {
        if (events & EventsTracker::EVENT_INPUT) {
            return mEventReceivedListenerFunc(fd, data);
        }
    }

    // Continued for client when no EventListener
    return EventsTracker::RET_CONTINUE;
}

int IpcSocket::handleServerEvents(int fd, int events, void *data) {
    auto *dataObj = static_cast<DataObj_t *>(data);
    auto ipcSocket = static_cast<IpcSocket *>(dataObj->obj1);

    if (fd == ipcSocket->mListenFd) {
        ALOGD("connect client coming");
        bool ok = ipcSocket->processListenFd(fd);

        // Do not hold self data anymore when success to
        // deliver it to message fd.
        dataObj->obj2 = nullptr;

        if (!ok) {
            return EventsTracker::RET_REMOVE;
        }
    } else {
        return ipcSocket->processFdFromClient(fd, events, dataObj->obj2);
    }
    return EventsTracker::RET_CONTINUE;
}

bool IpcSocket::send(int fd, const char *data, int size) {
    std::lock_guard<std::mutex> guard(mMutex);

    int sendLen = 0;
    int retLen = 0;
    int leftLen = size;
    const char *sendBuf = data;

    if (fd >= 0) {
        do {
            //prepare send length
            if (leftLen > SEND_MAX_LEN) {
                sendLen = SEND_MAX_LEN;
            } else {
                sendLen = leftLen;
            }

            //start to send
            retLen = ::send(fd, sendBuf, sendLen, 0);
            if (retLen < 0) {
                if (errno == EINTR) {
                    retLen = 0;
                }
                return false;
            }

            //prepare left buffer pointer
            sendBuf += retLen;
            leftLen -= retLen;
        } while (leftLen > 0);
    }

    return leftLen <= 0;
}

bool IpcSocket::receive(int fd, char *data, int size) {
    std::lock_guard<std::mutex> guard(mMutex);

    int retlen = 0;
    if (fd >= 0) {
        retlen = ::recv(fd, data, size, 0);
        if (retlen < 0) {
            if (errno == EINTR)
                retlen = 0;
            else {
                ALOGE("%s:%d, receive socket failed!", __FILE__, __LINE__);
                return false;
            }
        }
    }

    return retlen > 0;
}

void IpcSocket::join() {
    if (mEventsTracker != nullptr) {
        mEventsTracker->join();
    }
}

void IpcSocket::setEventListener(EventReceivedListener eventListener) {
    mEventReceivedListenerFunc = eventListener;
}

void IpcSocket::stop() {
    if (mEventsTracker != nullptr) {
        mEventsTracker->requestExit();
    }
}

std::string IpcSocket::sendForResult(int fd, const char *data, int size) {

    BlockChannel<std::string> blockChannel;
    EventReceivedListener tmpListener = nullptr;

    if (mEventReceivedListenerFunc != nullptr) {
        // Giving back after result is gotten
        tmpListener = mEventReceivedListenerFunc;
    }


    mEventReceivedListenerFunc = [&](int fd, void * /*selfData*/) -> int {
        char retBuf[1024];
        if (IpcSocket::receive(fd, retBuf, sizeof(retBuf))) {
            ALOGD("Client received: %s", retBuf);
            blockChannel.put(retBuf);
        } else {
            ALOGE("Server broken, remove");
            blockChannel.put("-1");
            return EventsTracker::RET_REMOVE;
        }

        return EventsTracker::RET_CONTINUE;
    };


    std::string ret;
    if (IpcSocket::send(fd, data, size)) {
        ret = blockChannel.poll();
    } else {
        ret = std::string("");
    }

    mEventReceivedListenerFunc = tmpListener;

    return ret;
}