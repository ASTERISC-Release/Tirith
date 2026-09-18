# Copyright (C) 2023 Gramine contributors
# SPDX-License-Identifier: BSD-3-Clause

CFLAGS = -Wall -Wextra
LDFLAGS = -ldl -lEGL -lGL -lgbm -ldrm -lxcb -lxcb-dri3 -lxcb-present -lxcb-sync -lxshmfence -lm -lX11 -L../../../sharedgl/build -I../../../sharedgl/inc/
INCLUDES = -I/usr/include/libdrm -I./

ifeq ($(DEBUG),1)
GRAMINE_LOG_LEVEL = debug
CFLAGS += -g
else
GRAMINE_LOG_LEVEL = error
CFLAGS += -O3
endif

.PHONY: all
all: egl_redirect glxgears.manifest helloworld

egl_redirect:
	make -C ../../../../egl-redirect

glxgears.manifest: glxgears.manifest.template
	gramine-manifest \
		-Dlog_level=$(GRAMINE_LOG_LEVEL) \
		$< $@
	gramine-manifest-check $@

helloworld: helloworld.o
	$(CC) helloworld.o -o helloworld $(LDFLAGS)

helloworld.o: main.c
	$(CC) -fPIC $(INCLUDES) -g -c $< -o $@

.PHONY: check
check: all
	$(GRAMINE) helloworld > OUTPUT
	echo "Hello, world" | diff OUTPUT -
	@echo "[ Success ]"

.PHONY: clean
clean:
	$(RM) *.token *.sig *.manifest.sgx *.manifest helloworld.o helloworld OUTPUT

.PHONY: distclean
distclean: clean
