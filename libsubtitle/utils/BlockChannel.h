/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
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
