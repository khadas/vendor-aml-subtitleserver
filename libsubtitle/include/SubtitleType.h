/* Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: header file
 */

#ifndef __SUBTITLETYPE_H
#define __SUBTITLETYPE_H

#include <memory>
#include <vector>
#include <binder/IMemory.h>
#include <binder/Parcel.h>

using android::IMemory;
using android::Parcel;

class  SubtitleHidlParcel {
public:
     void writeToParcel(Parcel* data){
        //Parcel data;
        int position = data->dataPosition();
        data->writeInt32(msgOwner);
        data->writeInt32(msgType);
        int length = bodyInt.size();
        int floatLenght = bodyFloat.size();
        int stringLength = bodyString.size();
        data->writeInt32(length);
        for(int i=0;i<length;i++){
            data->writeInt32(bodyInt[i]);
        }
        data->writeInt32(stringLength);
        for(int i=0;i<stringLength;i++){
            data->writeCString(bodyString[i].c_str());
        }
        data->writeInt32(floatLenght);
        for(int i=0;i<floatLenght;i++){
            data->writeFloat(bodyFloat[i]);
        }
        data->writeInt32(shmid);
        //data->setDataPosition(0);
  }

   void returnFromParcel(const Parcel& data){
        data.setDataPosition(0);
        msgOwner = data.readInt32();
        msgType = data.readInt32();
        int bodyLength = data.readInt32();
        bodyInt = std::vector<int>(bodyLength);
        for(int i=0;i<bodyLength;i++){
            bodyInt[i] = data.readInt32();
        }
        int stringLength = data.readInt32();
        for(int i=0;i<stringLength;i++){
            bodyString[i] = data.readCString();
        }
        int floatLength = data.readInt32();
        for(int i=0;i<floatLength;i++){
            bodyFloat[i] = data.readFloat();
        }
        shmid = data.readInt32();
        return;
 }

    uint32_t msgOwner; // which callback own this?
    uint32_t msgType;
    std::vector<int32_t> bodyInt;
    std::vector<std::string> bodyString;
    std::vector<float> bodyFloat;
    IMemory* mem;
    int shmid;
};

enum Result : uint32_t {
    OK,                  // Success
    FAIL,                // Failure, unknown reason
};

#endif
