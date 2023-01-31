#ifndef _DFBDEVICE_H
#define _DFBDEVICE_H

#ifdef USE_DFB
#include <utils/Log.h>

class DFBDevice  {
public:
    DFBDevice();
    virtual ~DFBDevice();

    bool initCheck() { return mInited; }
    void drawColor(float r, float g, float b, float a, bool flush = true);

private:
    bool init();

private:
    bool mInited{false};
};

#endif // USE_DFB
#endif //_DFBDEVICE_H
