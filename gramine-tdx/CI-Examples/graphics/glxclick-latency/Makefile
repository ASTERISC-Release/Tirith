# Copyright (C) 2026 Gramine contributors
# SPDX-License-Identifier: BSD-3-Clause

CC ?= cc
CFLAGS ?= -O2 -g
CFLAGS += -Wall -Wextra -Werror -I../../../../egl-redirect
EGL_REDIRECT := $(CURDIR)/../../../../egl-redirect/egl_redirect.so

.PHONY: all egl_redirect
all: egl_redirect glxclick-latency click_latency glxclick-latency.manifest

egl_redirect:
	$(MAKE) -C $(dir $(EGL_REDIRECT)) egl_redirect.so

glxclick-latency: main.o
	$(CC) $(LDFLAGS) -o $@ $^ -lGL -lX11

click_latency: click_latency.o
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

glxclick-latency.manifest: glxclick-latency.manifest.template
	gramine-manifest -Dintegration_dir="$(CURDIR)" $< $@
	gramine-manifest-check $@

.PHONY: run
run: all
	./run-experiment.sh

.PHONY: clean
clean:
	$(RM) *.o glxclick-latency click_latency glxclick-latency.manifest
