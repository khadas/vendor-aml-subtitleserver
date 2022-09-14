/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
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
