#ifdef USE_DFB

#define LOG_TAG "DFBDevice"
#include "DFBDevice.h"
#include <utils/Log.h>


DFBDevice::DFBDevice() {
    ALOGD("DFBDevice +++");
    mInited = init();
}

DFBDevice::~DFBDevice() {
    ALOGD("DFBDevice ---");
}

bool DFBDevice::init() {
    return false;
}


void DFBDevice::drawColor(float r, float g, float b, float a, bool flush) {

}
#endif