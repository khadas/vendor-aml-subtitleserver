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

#ifndef __SUBTITLE_SOCKETSERVER_H__
#define __SUBTITLE_SOCKETSERVER_H__
#include <utils/Log.h>
#include <utils/Thread.h>
#include <mutex>
#include <thread>
#include<vector>

#include "IpcDataTypes.h"
#include "DataSource.h"

#include "ringbuffer.h"

// TODO: use portable impl
using android::Thread;
using android::Mutex;

class EventsTracker;

/*socket communication, this definite need redesign. now only adaptor the older version */

class SubSocketServer {
public:
    SubSocketServer();
    ~SubSocketServer();

    // TODO: use smart ptr later
    static SubSocketServer* GetInstance();

    // TODO: error number
    int serve();
    bool isServing() { return mIsServing; }

    static const int LISTEN_PORT = 10100;
    static const int  QUEUE_SIZE = 10;

    static bool registClient(DataListener *client);
    static bool unregisterClient(DataListener *client);

    typedef struct DataObj {
        void * obj1 = nullptr;
        void * obj2 = nullptr;

        void reset(){
            obj1 = nullptr;
            obj2 = nullptr;
        }
    }DataObj_t;

private:
    // mimiced from android, free to rewrite if you don't like it
    bool threadLoop();
    void __threadLoop();

    static int handleEvent(int fd, int events, void* data);
    int processData(int fd, DataObj_t* dataObj);
    void onRemoteDead(int sessionId);

    // todo: impl client, not just a segment Reciver
    // todo: use key-value pair
    std::vector<DataListener *> mClients; //TODO: for clients
    std::shared_ptr<EventsTracker> mEventsTracker = nullptr;

    bool mExitRequested;
    bool mIsServing = false;

    std::thread mDispatchThread;

    std::mutex mLock;
    static SubSocketServer* mInstance;
};

#endif

