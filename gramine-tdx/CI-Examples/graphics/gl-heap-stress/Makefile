# SPDX-License-Identifier: BSD-3-Clause

CC ?= cc
CFLAGS ?= -O2 -g
CFLAGS += -Wall -Wextra $(shell sdl2-config --cflags)
LDLIBS += $(shell sdl2-config --libs) -lEGL -lGL -ldl -pthread

ifeq ($(DEBUG),1)
GRAMINE_LOG_LEVEL = debug
else
GRAMINE_LOG_LEVEL = error
endif

.PHONY: all egl_redirect clean
all: egl_redirect gl-heap-stress gl-heap-stress.manifest

egl_redirect:
	$(MAKE) -C ../../../../egl-redirect

gl-heap-stress: main.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

main.o: main.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

gl-heap-stress.manifest: gl-heap-stress.manifest.template
	gramine-manifest -Dlog_level=$(GRAMINE_LOG_LEVEL) $< $@
	gramine-manifest-check $@

clean:
	$(RM) gl-heap-stress main.o gl-heap-stress.manifest
