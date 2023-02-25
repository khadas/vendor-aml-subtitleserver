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

#ifndef _AML_MP_SIGNAL_HANDLER_H_
#define _AML_MP_SIGNAL_HANDLER_H_

#include <signal.h>
#include <ostream>
#include <map>

class SubtitleSignalHandler
{
public:
    static SubtitleSignalHandler& instance() {
        static SubtitleSignalHandler ins;
        return ins;
    }

    int installSignalHandlers();

    static void dumpstack(std::ostream& os);

private:
    SubtitleSignalHandler() = default;
    ~SubtitleSignalHandler() = default;

    static void unexpectedSignalHandler(int signal_number, siginfo_t* info, void*);
    static const char* getSignalName(int signal_number);
    static const char* getSignalCodeName(int signal_number, int signal_code);
    static std::string demangleSymbol(const char* symbol);

    std::map<int, void(*)(int)> mOldSignalHandlers;

private:
    SubtitleSignalHandler(const SubtitleSignalHandler&) = delete;
    SubtitleSignalHandler& operator=(const SubtitleSignalHandler&) = delete;
};
#endif
