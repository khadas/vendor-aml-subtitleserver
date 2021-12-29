//
// Created by jiajia.tian on 2021/11/17.
//

#ifndef _IPCBUFFER_H
#define _IPCBUFFER_H

#include "../utils/ringbuffer.h"
#include "IpcDataTypes.h"

class IpcBuffer {
public:
    IpcBuffer(size_t size, const char* name);
    ~IpcBuffer();

    void reset();
    int read(char* dst, int count);
    int peek(char* dst, int count);
    int write(char* src, int count);

    int readableCount();
    int writeableCount();

    const char* base();

    IpcPackageHeader* _data_header = nullptr;
private:
    rbuf_handle_t _handle = nullptr;
};


#endif //_IPCBUFFER_H
