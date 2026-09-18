# Copyright (C) 2023 Gramine contributors
# SPDX-License-Identifier: BSD-3-Clause

CFLAGS = -Wall -Wextra -std=c++17
LDFLAGS = -lX11 -lGL -lm

ifeq ($(DEBUG),1)
GRAMINE_LOG_LEVEL = debug
CFLAGS += -g
else
GRAMINE_LOG_LEVEL = error
CFLAGS += -O3
endif

.PHONY: all
all: cube.manifest cube


cube.manifest: cube.manifest.template
	gramine-manifest \
		-Dlog_level=$(GRAMINE_LOG_LEVEL) \
		$< $@
	gramine-manifest-check $@

cube: cube.o
	$(CXX) $(CFLAGS) cube.o -o cube $(LDFLAGS)

cube.o: cube.c
	$(CXX) -fPIC $(INCLUDES) $(CFLAGS) -g -c $< -o $@

.PHONY: check
check: all
	$(GRAMINE) helloworld > OUTPUT
	echo "Hello, world" | diff OUTPUT -
	@echo "[ Success ]"

.PHONY: clean
clean:
	$(RM) *.token *.sig *.manifest.sgx *.manifest cube.o cube

.PHONY: distclean
distclean: clean
