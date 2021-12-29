#ifndef LINUX_ISUBTITLECALLBACK_H
#define LINUX_ISUBTITLECALLBACK_H

#include <utils/RefBase.h>
#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include "SubtitleType.h"

using android::IBinder;
using android::BBinder;
using android::IInterface;
using android::BpInterface;
using android::BnInterface;
using android::Parcel;
using android::sp;
using android::status_t;


class ISubtitleCallback: public IInterface
{
public:
    DECLARE_META_INTERFACE(SubtitleCallback);

    virtual void notify(int msg, int ext1, int ext2, const Parcel *obj) = 0;
    virtual void notifyDataCallback(SubtitleHidlParcel& parcel) = 0;
    virtual void uiCommandCallback(SubtitleHidlParcel& parcel) = 0;
    virtual void eventNotify(SubtitleHidlParcel& parcel) = 0;
};

// ----------------------------------------------------------------------------

class BnSubtitleCallback: public BnInterface<ISubtitleCallback>
{
public:
    virtual status_t    onTransact( uint32_t code,
                                    const Parcel& data,
                                    Parcel* reply,
                                    uint32_t flags = 0);
};
#endif
