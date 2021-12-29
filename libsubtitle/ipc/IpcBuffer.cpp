#include "IpcBuffer.h"

IpcBuffer::IpcBuffer(size_t size, const char* name) {
    _handle = ringbuffer_create(size, name);
}

IpcBuffer::~IpcBuffer() {
    ringbuffer_free(_handle);
    _handle = nullptr;

    if (_data_header != nullptr) {
        delete _data_header;
        _data_header = nullptr;
    }
}

void IpcBuffer::reset() {
    ringbuffer_cleanreset(_handle);

    if (_data_header != nullptr) {
        delete _data_header;
        _data_header = nullptr;
    }
}

int IpcBuffer::read(char *dst, int count) {
    return ringbuffer_read(_handle, dst, count, rbuf_mode_t::RBUF_MODE_BLOCK);
}

int IpcBuffer::peek(char *dst, int count) {
    return ringbuffer_peek(_handle, dst, count, rbuf_mode_t::RBUF_MODE_BLOCK);
}

int IpcBuffer::write(char *src, int count) {
    return ringbuffer_write(_handle, src, count, rbuf_mode_t::RBUF_MODE_BLOCK);
}

int IpcBuffer::readableCount() {
    return ringbuffer_read_avail(_handle);
}

int IpcBuffer::writeableCount() {
    return ringbuffer_write_avail(_handle);
}

const char *IpcBuffer::base() {
    return static_cast<const char *>(_handle);
}
