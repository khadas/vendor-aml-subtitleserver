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

#define LOG_TAG "SubtitleSignalHandler"
#include "SubtitleSignalHandler.h"
#include "SubtitleLog.h"
#include <string.h>
#include <unistd.h>
#include <execinfo.h>

#include <string>
#include <sstream>
#include <iomanip>
#include <cxxabi.h>

static const char* mName = LOG_TAG;

int SubtitleSignalHandler::installSignalHandlers()
{
    int ret = 0;
    struct sigaction act, old;

    memset(&act, 0, sizeof(struct sigaction));
    sigemptyset(&act.sa_mask);
    act.sa_flags |= SA_SIGINFO | SA_ONSTACK;
    act.sa_sigaction = unexpectedSignalHandler;

    ret  = sigaction(SIGABRT, &act, &old);
    if (ret == 0) instance().mOldSignalHandlers[SIGABRT] = old.sa_handler;

    ret += sigaction(SIGBUS, &act, &old);
    if (ret == 0) instance().mOldSignalHandlers[SIGBUS] = old.sa_handler;

    ret += sigaction(SIGFPE, &act, &old);
    if (ret == 0) instance().mOldSignalHandlers[SIGFPE] = old.sa_handler;

    ret += sigaction(SIGILL, &act, &old);
    if (ret == 0) instance().mOldSignalHandlers[SIGILL] = old.sa_handler;

    ret += sigaction(SIGSEGV, &act, &old);
    if (ret == 0) instance().mOldSignalHandlers[SIGSEGV] = old.sa_handler;

    ret += sigaction(SIGTRAP, &act, &old);
    if (ret == 0) instance().mOldSignalHandlers[SIGTRAP] = old.sa_handler;

    return 0;
}

void SubtitleSignalHandler::unexpectedSignalHandler(int signal_number, siginfo_t *info, void* )
{
    static bool handlingUnexpectedSignal = false;
    if (handlingUnexpectedSignal) {
        SUBTITLE_LOGE("HandleUnexpectedSignal reentered\n");

        _exit(1);
    }
    handlingUnexpectedSignal = true;

    bool has_address = (signal_number == SIGILL || signal_number == SIGBUS ||
                        signal_number == SIGFPE || signal_number == SIGSEGV);

    std::stringstream ss;
    ss << "\n  *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***\n"
       << "  Fatal signal " << signal_number << " (" << getSignalName(signal_number) << "), code "
       << info->si_code << " (" << getSignalCodeName(signal_number, info->si_code) << ")";
    if (has_address) {
        ss << ", fault addr " << std::hex << info->si_addr << "\n";
    }

    instance().dumpstack(ss);
    SUBTITLE_LOGE("%s", ss.str().c_str());
    fprintf(stderr, "%s\n", ss.str().c_str());

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    sigemptyset(&action.sa_mask);
    action.sa_handler = instance().mOldSignalHandlers[signal_number];
    sigaction(signal_number, &action, NULL);
    // ...and re-raise so we die with the appropriate status.
    kill(getpid(), signal_number);
}

const char* SubtitleSignalHandler::getSignalName(int signal_number)
{
    switch (signal_number) {
      case SIGABRT: return "SIGABRT";
      case SIGBUS: return "SIGBUS";
      case SIGFPE: return "SIGFPE";
      case SIGILL: return "SIGILL";
      case SIGPIPE: return "SIGPIPE";
      case SIGSEGV: return "SIGSEGV";
  #if defined(SIGSTKFLT)
      case SIGSTKFLT: return "SIGSTKFLT";
  #endif
      case SIGTRAP: return "SIGTRAP";
    }
    return "??";
}

const char* SubtitleSignalHandler::getSignalCodeName(int signal_number, int signal_code)
{
    // Try the signal-specific codes...
    switch (signal_number) {
      case SIGILL:
        switch (signal_code) {
          case ILL_ILLOPC: return "ILL_ILLOPC";
          case ILL_ILLOPN: return "ILL_ILLOPN";
          case ILL_ILLADR: return "ILL_ILLADR";
          case ILL_ILLTRP: return "ILL_ILLTRP";
          case ILL_PRVOPC: return "ILL_PRVOPC";
          case ILL_PRVREG: return "ILL_PRVREG";
          case ILL_COPROC: return "ILL_COPROC";
          case ILL_BADSTK: return "ILL_BADSTK";
        }
        break;
      case SIGBUS:
        switch (signal_code) {
          case BUS_ADRALN: return "BUS_ADRALN";
          case BUS_ADRERR: return "BUS_ADRERR";
          case BUS_OBJERR: return "BUS_OBJERR";
        }
        break;
      case SIGFPE:
        switch (signal_code) {
          case FPE_INTDIV: return "FPE_INTDIV";
          case FPE_INTOVF: return "FPE_INTOVF";
          case FPE_FLTDIV: return "FPE_FLTDIV";
          case FPE_FLTOVF: return "FPE_FLTOVF";
          case FPE_FLTUND: return "FPE_FLTUND";
          case FPE_FLTRES: return "FPE_FLTRES";
          case FPE_FLTINV: return "FPE_FLTINV";
          case FPE_FLTSUB: return "FPE_FLTSUB";
        }
        break;
      case SIGSEGV:
        switch (signal_code) {
          case SEGV_MAPERR: return "SEGV_MAPERR";
          case SEGV_ACCERR: return "SEGV_ACCERR";
        }
        break;
      case SIGTRAP:
        switch (signal_code) {
          case TRAP_BRKPT: return "TRAP_BRKPT";
          case TRAP_TRACE: return "TRAP_TRACE";
        }
        break;
    }
    // Then the other codes...
    switch (signal_code) {
      case SI_USER:     return "SI_USER";
  #if defined(SI_KERNEL)
      case SI_KERNEL:   return "SI_KERNEL";
  #endif
      case SI_QUEUE:    return "SI_QUEUE";
      case SI_TIMER:    return "SI_TIMER";
      case SI_MESGQ:    return "SI_MESGQ";
      case SI_ASYNCIO:  return "SI_ASYNCIO";
  #if defined(SI_SIGIO)
      case SI_SIGIO:    return "SI_SIGIO";
  #endif
  #if defined(SI_TKILL)
      case SI_TKILL:    return "SI_TKILL";
  #endif
    }
    // Then give up...
    return "?";
}

void SubtitleSignalHandler::dumpstack(std::ostream& os)
{
  char* stack[20] = {0};
  int depth = backtrace(reinterpret_cast<void**>(stack), sizeof(stack)/sizeof(stack[0]));
  if (depth) {
      char** symbols = backtrace_symbols(reinterpret_cast<void**>(stack), depth);
      if (symbols) {
          os << std::dec;
          for (int i = 0; i < depth; i++) {
              os << "#" << std::setw(2) << std::left << i + 1 << " " << demangleSymbol(symbols[i]) << "\n";
          }
      }
      free(symbols);
  }
}

std::string SubtitleSignalHandler::demangleSymbol(const char* symbol)
{
    std::string result;
    const char* p = strchr(symbol, '(');
    if (p == nullptr) {
        return symbol;
    }

    std::string libraryName(symbol, p - symbol);
    result.append(libraryName);

    const char* pSymbolName = p +1;
    p = strchr(pSymbolName, '+');
    if (p == nullptr) {
        return symbol;
    }

    std::string symbolName(pSymbolName, p - pSymbolName);

    int status;
    char* demangleResult = abi::__cxa_demangle(symbolName.c_str(), nullptr, nullptr, &status);
    if (status == 0) {
        result.append("(");
        result.append(demangleResult);
        result.append(p);
    }
    free(demangleResult);

    return result;
}
