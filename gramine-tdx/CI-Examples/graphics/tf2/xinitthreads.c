// Copyright (C) 2026 Gramine contributors
// SPDX-License-Identifier: BSD-3-Clause

#include <X11/Xlib.h>

__attribute__((constructor)) static void initialize_xlib_threads(void) {
    XInitThreads();
}
