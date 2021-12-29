// currently, we use android thread
// TODO: impl with std lib, make it portable
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
    static bool unregistClient(DataListener *client);

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

