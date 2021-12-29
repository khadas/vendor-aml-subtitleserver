/*
**
** Copyright 2008, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include <utils/RefBase.h>
#include <binder/IInterface.h>
#include <binder/Parcel.h>

#include "ISubtitleCallback.h"


enum {
    NOTIFY = IBinder::FIRST_CALL_TRANSACTION,
    DATA_CALLBACK = IBinder::FIRST_CALL_TRANSACTION+1,
    UI_CALLBACK = IBinder::FIRST_CALL_TRANSACTION+2,
    EVENT_CALLBACK = IBinder::FIRST_CALL_TRANSACTION+3,
};

class BpSubtitleCallback: public BpInterface<ISubtitleCallback>
{
public:
    explicit BpSubtitleCallback(const sp<IBinder>& impl)
        : BpInterface<ISubtitleCallback>(impl)
    {
    }

    virtual void notifyDataCallback(SubtitleHidlParcel& parcel)
    {
        Parcel data, reply;
             parcel.writeToParcel(&data);
        remote()->transact(DATA_CALLBACK, data, &reply, IBinder::FLAG_ONEWAY);
    }

    virtual void uiCommandCallback(SubtitleHidlParcel& parcel)
    {
        Parcel data, reply;
        data.writeInterfaceToken(ISubtitleCallback::getInterfaceDescriptor());
        parcel.writeToParcel(&data);
        remote()->transact(UI_CALLBACK, data, &reply, IBinder::FLAG_ONEWAY);
    }

    virtual void eventNotify(SubtitleHidlParcel& parcel)
    {
        Parcel data, reply;
        data.writeInterfaceToken(ISubtitleCallback::getInterfaceDescriptor());
        parcel.writeToParcel(&data);
         remote()->transact(EVENT_CALLBACK, data, &reply, IBinder::FLAG_ONEWAY);
    }



    virtual void notify(int msg, int ext1, int ext2, const Parcel *obj)
    {
        Parcel data, reply;
        data.writeInterfaceToken(ISubtitleCallback::getInterfaceDescriptor());
        data.writeInt32(msg);
        data.writeInt32(ext1);
        data.writeInt32(ext2);
        if (obj && obj->dataSize() > 0) {
            data.appendFrom(const_cast<Parcel *>(obj), 0, obj->dataSize());
        }
        remote()->transact(NOTIFY, data, &reply, IBinder::FLAG_ONEWAY);
    }
};

IMPLEMENT_META_INTERFACE(SubtitleCallback, "aml_subtitleserver.ISubtitleCallback");

// ----------------------------------------------------------------------
//     BnSubtitleCallback::onTransact(unsigned int, android::Parcel const&, android::Parcel*, unsigned int)
status_t BnSubtitleCallback::onTransact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    switch (code) {
        case NOTIFY:
        {
            int msg = data.readInt32();
            int ext1 = data.readInt32();
            int ext2 = data.readInt32();
            Parcel obj;
            if (data.dataAvail() > 0) {
                obj.appendFrom(const_cast<Parcel *>(&data), data.dataPosition(), data.dataAvail());
            }

            notify(msg, ext1, ext2, &obj);
        }
            break;
         case DATA_CALLBACK:
         {
             SubtitleHidlParcel temp;
             SubtitleHidlParcel& para = temp;
             temp.returnFromParcel(data);
             notifyDataCallback(para);
         }
             break;
         case UI_CALLBACK:
         {
             SubtitleHidlParcel temp;
             SubtitleHidlParcel& para = temp;
             temp.returnFromParcel(data);
             uiCommandCallback(para);
         }
             break;
         case EVENT_CALLBACK:
         {
             SubtitleHidlParcel temp;
             SubtitleHidlParcel& para = temp;
             temp.returnFromParcel(data);
             eventNotify(para);
         }
             break;
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
    return 0;
}

