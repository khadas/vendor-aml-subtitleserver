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

#ifndef IPCSOCKET_H
#define IPCSOCKET_H

#include <iostream>
#include <mutex>
#include <memory>
#include <functional>
#include <EventsTracker.h>

class EventsTracker;

using EventReceivedListener = std::function<int(int, void*)>;

class IpcSocket {
public:
    typedef struct DataObj {
        void *obj1 = nullptr;
        void *obj2 = nullptr;

        void reset() {
            obj1 = nullptr;
            obj2 = nullptr;
        }
    } DataObj_t;

public:
    explicit IpcSocket(bool isServer);

    ~IpcSocket();

    void start(int port, void *selfData, OnThreadStart onThreadStart = nullptr);

    bool isvalid();

    void setEventListener(EventReceivedListener eventListener);

    static bool send(int fd, const char *data, int size);
    static bool receive(int fd, char *data, int size);

    std::string sendForResult(int fd, const char *data, int size);


    void join();
    void stop();

    int fd() { return mListenFd;}

private:
    static int handleEvents(int fd, int events, void *data);

    int handleServerEvents(int fd, int events, void *data);

    int handleClientEvents(int fd, int events, void *data);

    bool initAsServer(int port);

    bool initAsClient(int port);

    bool processListenFd(int fd);

    int processFdFromClient(int fd, int events, void *data);

private:
    bool mIsServer;
    bool mIsValid;

    int mListenFd = -1;

    void* mSelfData = nullptr;

    std::shared_ptr<EventsTracker> mEventsTracker;
    EventReceivedListener mEventReceivedListenerFunc;


};


#endif //IPCSOCKET_H
