/************************************************
 * name : AmSocket.cpp
 * function : transfer data by socket
 * data : 2017.03.02
 * author : wxl
 * version  : 1.0.0
 *************************************************/
#define LOG_NDEBUG 0
#define LOG_TAG "AmSocket"
#include <utils/Log.h>

#include <arpa/inet.h>
#include <errno.h>
//#include <media/stagefright/foundation/ALooper.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <unistd.h>
#include <chrono>

#include "AmSocketClient.h"
#include "cutils/properties.h"
//#include "utils/AmlMpLog.h"


static const char* mName = "AmSocketClient";

//namespace aml_mp {

AmSocketClient::AmSocketClient() {
    mSockFd = -1;
}

AmSocketClient::~AmSocketClient() {
    mSockFd = -1;
}

int64_t AmSocketClient::GetNowUs() {
    //return systemTime(SYSTEM_TIME_MONOTONIC) / 1000LL;
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

int AmSocketClient::socketConnect() {
    ALOGD("socketConnect");

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    //inet_aton("127.0.0.1", (struct in_addr *)&addr.sin_addr);
    addr.sin_port = htons(SERVER_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    mSockFd = socket(AF_INET, SOCK_STREAM, 0);
    if (mSockFd < 0) {
	    //MLOGI("%s:%d, create socket failed!mSockFd:%d, error=%d, err:%s\n", __FILE__, __LINE__, mSockFd, errno, strerror(errno));
        ALOGE("%s:%d, create socket failed!mSockFd:%d, error=%d, err:%s\n", __FILE__, __LINE__, mSockFd, errno, strerror(errno));
        return -1;
    }
    //MLOGI("%s:%d, create socket success!mSockFd:%d\n", __FILE__, __LINE__, mSockFd);

    //fix upgrade first play connect fail because of early conntect ,add while strategy for 40ms.
    int64_t startTime = GetNowUs();
    while (true) {
        if (::connect(mSockFd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            //load dvb so need time since add close caption subtitle, add to 120ms.
            //only has subtitle to connect socket
            if ( GetNowUs() - startTime > 120000ll) {//systemTime(SYSTEM_TIME_MONOTONIC) / 1000LL;
                //MLOGI("%s:%d, connect socket failed!, error=%d, err:%s\n", __FILE__, __LINE__, errno, strerror(errno));
                ALOGE("%s:%d, connect socket failed!, error=%d, err:%s\n", __FILE__, __LINE__, errno, strerror(errno));
                close(mSockFd);
                mSockFd = -1;
                return -1;
            }
        } else {
           break;
        }
    }

    ALOGI("%s:%d, connect socket success!mSockFd:%d\n", __FILE__, __LINE__, mSockFd);

    return 0;
}

void AmSocketClient::socketSend(const char *buf, int size) {
    android::AutoMutex _l(mSockLock);
    char value[PROPERTY_VALUE_MAX] = {0};
    if (property_get("sys.amsocket.disable", value, "false") > 0) {
        if (!strcmp(value, "true")) {
            return;
        }
    }
    int sendLen = 0;
    int retLen = 0;
    int leftLen = size;
    const char *sendBuf = buf;
    char recvBuf[32] = {0};

    if (mSockFd >= 0) {
        do {
            //prepare send length
            if (leftLen > SEND_LEN) {
                sendLen = SEND_LEN;
            }
            else {
                sendLen = leftLen;
            }

            //start to send
            retLen = send(mSockFd, sendBuf, sendLen, 0);
            if (retLen < 0) {
                if (errno == EINTR) {
                    retLen = 0;
                }
		     //MLOGI("%s:%d, send socket failed!retLen:%d\n", __FILE__, __LINE__, retLen);
                return;
            }

            //prepare left buffer pointer
            sendBuf += retLen;
            leftLen -= retLen;
        } while (leftLen > 0);
    }
}

int AmSocketClient::socketRecv(char *buf, int size) {
    android::AutoMutex _l(mSockLock);
    int retlen = 0;
    if (mSockFd >= 0) {
        retlen = recv(mSockFd, buf, size, 0);
        if (retlen < 0) {
            if (errno == EINTR)
                retlen = 0;
            else {
                ALOGE("%s:%d, receive socket failed!", __FILE__, __LINE__);
                return -1;
            }
        }
    }

    return retlen;
}

void AmSocketClient::socketDisconnect() {
    if (mSockFd >= 0) {
        close(mSockFd);
        mSockFd = -1;
    }
}

//}
