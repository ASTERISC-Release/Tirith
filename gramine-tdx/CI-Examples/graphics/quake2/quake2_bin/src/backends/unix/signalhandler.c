/*
 * Copyright (C) 2011 Yamagi Burmeister
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * =======================================================================
 *
 * This is a signal handler for printing some hints to debug problem in
 * the case of a crash. On Linux a backtrace is printed. Additionally
 * a special handler for SIGINT and SIGTERM is supplied.
 *
 * =======================================================================
 */

#define _GNU_SOURCE
#include <signal.h>
#include <ucontext.h>

#include "../../common/header/common.h"

#if defined(HAVE_EXECINFO)
#include <execinfo.h>

void printBacktrace(int sig) {
    void* array[15];
    size_t size;
    char** strings;
    int i;

    size    = backtrace(array, 15);
    strings = backtrace_symbols(array, size);

    printf("Product:      Yamagi Quake II\n");
    printf("Version:      %s\n", YQ2VERSION);
    printf("Platform:     %s\n", YQ2OSTYPE);
    printf("Architecture: %s\n", YQ2ARCH);
    printf("Compiler:     %s\n", __VERSION__);
    printf("Signal:       %i\n", sig);
    printf("\nBacktrace:\n");

    for (i = 0; i < size; i++) {
        printf("  %s\n", strings[i]);
    }

    printf("\n");
}

#else

void printBacktrace(int sig) {
    printf("Product:      Yamagi Quake II\n");
    printf("Version:      %s\n", YQ2VERSION);
    printf("Platform:     %s\n", YQ2OSTYPE);
    printf("Architecture: %s\n", YQ2ARCH);
    printf("Compiler:     %s\n", __VERSION__);
    printf("Signal:       %i\n", sig);
    printf("\nBacktrace:\n");
    printf("  Not available on this platform.\n\n");
}

#endif

void signalhandler(int sig) {
    printf("\n=======================================================\n");
    printf("\nYamagi Quake II crashed! This should not happen...\n");
    printf("\nMake sure that you're using the last version. It can\n");
    printf("be found at http://www.yamagi.org/quake2. If you do,\n");
    printf("send a bug report to quake2@yamagi.org and include:\n\n");
    printf(" - This output\n");
    printf(" - The conditions that triggered the crash\n");
    printf(" - How to reproduce the crash (if known)\n");
    printf(" - The following files. None of them contains private\n");
    printf("   data. They're necessary to analyze the backtrace:\n\n");
    printf("    - quake2 (the executable / binary)\n\n");
    printf("    - game.so (the game.so of the mod you were playing\n");
    printf("      when the game crashed. baseq2/game.so for the\n");
    printf("      main game)\n\n");
    printf(" - Any other data which you think might be useful\n");
    printf("\nThank you very much for your help, making Yamagi Quake\n");
    printf("II an even better source port. It's much appreciated.\n");
    printf("\n=======================================================\n\n");

    printBacktrace(sig);

    /* make sure this is written */
    fflush(stdout);

    /* reset signalhandler */
    signal(SIGSEGV, SIG_DFL);
    signal(SIGILL, SIG_DFL);
    signal(SIGFPE, SIG_DFL);
    signal(SIGABRT, SIG_DFL);
    signal(SIGBUS, SIG_DFL);

    /* pass signal to the os */
    raise(sig);
}

void sigsegvhandler(int sig, siginfo_t* si, void* arg) {
    printf("\n=======================================================\n");
    printf("\nYamagi Quake II crashed! This should not happen...\n");
    printf("\nMake sure that you're using the last version. It can\n");
    printf("be found at http://www.yamagi.org/quake2. If you do,\n");
    printf("send a bug report to quake2@yamagi.org and include:\n\n");
    printf(" - This output\n");
    printf(" - The conditions that triggered the crash\n");
    printf(" - How to reproduce the crash (if known)\n");
    printf(" - The following files. None of them contains private\n");
    printf("   data. They're necessary to analyze the backtrace:\n\n");
    printf("    - quake2 (the executable / binary)\n\n");
    printf("    - game.so (the game.so of the mod you were playing\n");
    printf("      when the game crashed. baseq2/game.so for the\n");
    printf("      main game)\n\n");
    printf(" - Any other data which you think might be useful\n");
    printf("\nThank you very much for your help, making Yamagi Quake\n");
    printf("II an even better source port. It's much appreciated.\n");
    printf("\n=======================================================\n\n");

    ucontext_t* uc = (ucontext_t*)arg;

    printf("Caught signal %d (%s)\n", sig, strsignal(sig));
    if (si) {
        printf("si_addr (faulting addr): %p\n", si->si_addr);
        printf("si_code: %d\n", si->si_code);
    }

    greg_t* gregs = uc->uc_mcontext.gregs;
    printf("---- registers (x86_64) ----\n");
    printf("RIP: 0x%llx\n", (unsigned long long)gregs[REG_RIP]);
    printf("RSP: 0x%llx\n", (unsigned long long)gregs[REG_RSP]);
    printf("RBP: 0x%llx\n", (unsigned long long)gregs[REG_RBP]);
    printf("RAX: 0x%llx\n", (unsigned long long)gregs[REG_RAX]);
    printf("RBX: 0x%llx\n", (unsigned long long)gregs[REG_RBX]);
    printf("RCX: 0x%llx\n", (unsigned long long)gregs[REG_RCX]);
    printf("RDX: 0x%llx\n", (unsigned long long)gregs[REG_RDX]);
    printf("RSI: 0x%llx\n", (unsigned long long)gregs[REG_RSI]);
    printf("RDI: 0x%llx\n", (unsigned long long)gregs[REG_RDI]);
    printf("R8 : 0x%llx\n", (unsigned long long)gregs[REG_R8]);
    printf("R9 : 0x%llx\n", (unsigned long long)gregs[REG_R9]);
    printf("R10: 0x%llx\n", (unsigned long long)gregs[REG_R10]);
    printf("R11: 0x%llx\n", (unsigned long long)gregs[REG_R11]);
    printf("R12: 0x%llx\n", (unsigned long long)gregs[REG_R12]);
    printf("R13: 0x%llx\n", (unsigned long long)gregs[REG_R13]);
    printf("R14: 0x%llx\n", (unsigned long long)gregs[REG_R14]);
    printf("R15: 0x%llx\n", (unsigned long long)gregs[REG_R15]);

    printBacktrace(sig);

    /* make sure this is written */
    fflush(stdout);

    /* reset signalhandler */
    signal(SIGSEGV, SIG_DFL);
    signal(SIGILL, SIG_DFL);
    signal(SIGFPE, SIG_DFL);
    signal(SIGABRT, SIG_DFL);
    signal(SIGBUS, SIG_DFL);

    /* pass signal to the os */
    raise(sig);
}

extern qboolean quitnextframe;

void terminate(int sig) {
    quitnextframe = true;
}

void registerHandler(void) {
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = sigsegvhandler;
    sa.sa_flags     = SA_SIGINFO | SA_RESTART;

    /* Crash: Segmentation Fault */
    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    /* Crash: Others */
    // signal(SIGSEGV, signalhandler);
    signal(SIGILL, signalhandler);
    signal(SIGFPE, signalhandler);
    signal(SIGABRT, signalhandler);
    signal(SIGBUS, signalhandler);

    /* User abort */
    signal(SIGINT, terminate);
    signal(SIGTERM, terminate);
}
