# Copyright (C) 2023 Gramine contributors
# SPDX-License-Identifier: BSD-3-Clause

CFLAGS = -Wall -Wextra -lX11 -lGL -L../../../sharedgl/build -I../../../sharedgl/inc/
LDFLAGS = -lX11 -lGL -L../../../sharedgl/build -I../../../sharedgl/inc/

ifeq ($(DEBUG),1)
GRAMINE_LOG_LEVEL = debug
CFLAGS += -g
else
GRAMINE_LOG_LEVEL = error
CFLAGS += -O3
endif

.PHONY: all
all: egl_redirect glxgears.manifest

egl_redirect:
	make -C ../../../../egl-redirect

glxgears.manifest: glxgears.manifest.template
	gramine-manifest \
		-Dlog_level=$(GRAMINE_LOG_LEVEL) \
		$< $@
	gramine-manifest-check $@

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
