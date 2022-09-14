/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
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
