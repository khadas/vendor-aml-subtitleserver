#ifndef AM_SOCKET_CLIENT_H
#define AM_SOCKET_CLIENT_H
#include <utils/Mutex.h>
//namespace aml_mp {
//------------------------------------------------------------------------------
#define SEND_LEN 1024
#define SERVER_PORT 10100

class AmSocketClient
{
public:
    AmSocketClient();

    ~AmSocketClient();

    int socketConnect();

    void socketSend(const char *buf, int size);

    int socketRecv(char *buf, int size);

    void socketDisconnect();

    static int64_t GetNowUs();

private:
    int mSockFd;
    mutable android::Mutex mSockLock;
};
//------------------------------------------------------------------------------
//}
#endif
