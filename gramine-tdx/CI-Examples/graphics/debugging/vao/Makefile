# Copyright (C) 2023 Gramine contributors
# SPDX-License-Identifier: BSD-3-Clause

CFLAGS = -Wall -Wextra -lX11 -lGL -L../../../sharedgl/build -I../../../sharedgl/inc/
LDFLAGS = -lX11 -lGL -L../../../sharedgl/build -I../../../sharedgl/inc/ -lm

ifeq ($(DEBUG),1)
GRAMINE_LOG_LEVEL = debug
CFLAGS += -g
else
GRAMINE_LOG_LEVEL = error
CFLAGS += -O3
endif

.PHONY: all
all: example-with-vao.manifest example-with-vao


example-with-vao.manifest: example-with-vao.manifest.template
	gramine-manifest \
		-Dlog_level=$(GRAMINE_LOG_LEVEL) \
		$< $@
	gramine-manifest-check $@

example-with-vao: example-with-vao.o
	$(CC) example-with-vao.o -o example-with-vao $(LDFLAGS)

example-with-vao.o: example-with-vao.c
	$(CC) -fPIC $(INCLUDES) -g -c $< -o $@

.PHONY: check
check: all
	$(GRAMINE) helloworld > OUTPUT
	echo "Hello, world" | diff OUTPUT -
	@echo "[ Success ]"

.PHONY: clean
clean:
	$(RM) *.token *.sig *.manifest.sgx *.manifest

.PHONY: distclean
distclean: clean
