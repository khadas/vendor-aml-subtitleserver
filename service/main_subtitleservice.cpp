/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <binder/IServiceManager.h>
#include "SubtitleServiceLinux.h"


int main(int argc, char **argv) {
   SubtitleServiceLinux *mpSubtitleService = SubtitleServiceLinux::GetInstance();
   sp<ProcessState> proc(ProcessState::self());

   sp<IServiceManager> serviceManager = defaultServiceManager();
   serviceManager->addService(String16("subtitleservice"), mpSubtitleService);

   proc->startThreadPool();
   IPCThreadState::self()->joinThreadPool();
}

