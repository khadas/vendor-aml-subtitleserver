/*
 * Copyright (C) 2014-2019 Amlogic, Inc. All rights reserved.
 *
 * All information contained herein is Amlogic confidential.
 *
 * This software is provided to you pursuant to Software License Agreement
 * (SLA) with Amlogic Inc ("Amlogic"). This software may be used
 * only in accordance with the terms of this agreement.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification is strictly prohibited without prior written permission from
 * Amlogic.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

