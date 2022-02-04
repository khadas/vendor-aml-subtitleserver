#ifndef AM_SOCKET_CLIENT_H
#define AM_SOCKET_CLIENT_H
#include <utils/Mutex.h>
//namespace aml_mp {
//------------------------------------------------------------------------------
#define SEND_LEN 1024
#define SERVER_PORT 10100
#ifdef RDK_AML_SUBTITLE_SOCKET
    #define SERVER_PORT_CMD 10200
#endif  //RDK_AML_SUBTITLE_SOCKET

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

    #ifdef RDK_AML_SUBTITLE_SOCKET
    int socketConnectCmd();
    int socketSendCmd(const char *buf, int size);
    int socketRecvCmd(char *buf, int size);
    #endif //RDK_AML_SUBTITLE_SOCKET
private:
    int mSockFd;
    mutable android::Mutex mSockLock;

    #ifdef RDK_AML_SUBTITLE_SOCKET
    int mCmdSockFd;
    mutable android::Mutex mCmdSockLock;
    #endif //RDK_AML_SUBTITLE_SOCKET
};
//------------------------------------------------------------------------------
//}
#endif
