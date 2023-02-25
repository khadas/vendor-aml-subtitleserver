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

#ifndef BLOCKCHANNEL_H
#define BLOCKCHANNEL_H

#include <mutex>
#include <condition_variable>
#include <list>

using Mutext = std::unique_lock<std::mutex>;

template <typename T>
class BlockChannel {
public:
    BlockChannel(int maxSize = 1) : mMaxSize(maxSize) {}
    ~BlockChannel(){
        if (mList.size() > 0) {
            mList.clear();
        }
    }

    void put(T item) {
        Mutext mutex(mMutex);

        if (mList.size() >= mMaxSize) {
            mCond.wait(mutex);
            mList.emplace_back(item);
        } else if (mList.size() <= 0) {
            mList.emplace_back(item);
            mCond.notify_all();
        } else {
            mList.emplace_back(item);
        }

    }

    T poll() {
        Mutext mutex(mMutex);

        T item;
        if (mList.size() <= 0) {
            mCond.wait(mutex);
            item = mList.front();
        } else if (mList.size() == mMaxSize) {
            item = mList.front();
            mCond.notify_all();
        } else {
            item = mList.front();
        }

        mList.pop_front();

        return item;
    }


private:
    int mMaxSize;
    std::list<T> mList;
    std::mutex mMutex;
    std::condition_variable mCond;
};

#endif //BLOCKCHANNEL_H
