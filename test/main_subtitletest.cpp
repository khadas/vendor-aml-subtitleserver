/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#define LOG_MOUDLE_TAG "Subtitle"
#define LOG_CLASS_TAG "SubtitleTest"

#include <syslog.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
//#include <SubtitleClientLinux.h>
//#include <SubtitleNativeAPI.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>


class SubtitleTest {
public:
    SubtitleTest()
    {
       // mpSubtitleClient = SubtitleClientLinux::GetInstance();
    }

    ~SubtitleTest()
    {

    }


    //SubtitleClientLinux *mpSubtitleClient;
    int setValue[10] = {0};
};


int main(int argc, char **argv) {
 //   unsigned char read_buf[256];
//    memset(read_buf, 0, sizeof(read_buf));
 //    ALOGD("AAAAAAAAAAAAAAAA subtitletest  %s, line %d",__FUNCTION__,__LINE__);

//ALOGD("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA main subtitletest.cpp AAAAAAAAAAAAAAAAA");
    //sp<ProcessState> proc(ProcessState::self());
    //proc->startThreadPool();
	//amlsub_Create();
   SubtitleTest *test = new SubtitleTest();
 //   int run = 1;


    return 0;
}
