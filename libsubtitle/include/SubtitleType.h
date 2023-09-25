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

#ifndef __SUBTITLETYPE_H
#define __SUBTITLETYPE_H

#include <memory>
#include <vector>
#include <binder/IMemory.h>
#include <binder/Parcel.h>
#include <string>

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
