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

#define LOG_TAG "Watchdog"

#include "Watchdog.h"
#include "SubtitleLog.h"

#include <signal.h>
#include <time.h>
#include <cstring>
#include <utils/Log.h>


Watchdog::Watchdog(::std::chrono::steady_clock::duration timeout) {
    #ifdef NEED_WATCHDOG
    // Create the timer.
    struct sigevent sev;
    sev.sigev_notify = SIGEV_THREAD_ID;
    //sev.sigev_notify_thread_id = android::base::GetThreadId();
    sev.sigev_signo = SIGABRT;
    sev.sigev_value.sival_ptr = &mTimerId;
    int err = timer_create(CLOCK_MONOTONIC, &sev, &mTimerId);
    if (err != 0) {
        SUBTITLE_LOGE("Failed to create timer");
    }

    // Start the timer.
    struct itimerspec spec;
    memset(&spec, 0, sizeof(spec));
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(timeout);
    LOG_ALWAYS_FATAL_IF(timeout.count() <= 0, "Duration must be positive");
    spec.it_value.tv_sec = ns.count() / 1000000000;
    spec.it_value.tv_nsec = ns.count() % 1000000000;
    err = timer_settime(mTimerId, 0, &spec, nullptr);
    if (err != 0) {
        SUBTITLE_LOGE("Failed to start timer");
    }
    #else
    SUBTITLE_LOGI("%s empty implementation.", __func__);
    #endif
}

Watchdog::~Watchdog() {
    #ifdef NEED_WATCHDOG
    // Delete the timer.
    int err = timer_delete(mTimerId);
    if (err != 0) {
         SUBTITLE_LOGE("Failed to delete timer");
    }
    #else
    SUBTITLE_LOGI("%s empty implementation.", __func__);
    #endif
}
