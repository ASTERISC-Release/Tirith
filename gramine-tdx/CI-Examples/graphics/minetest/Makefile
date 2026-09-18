# Copyright (...)

CFLAGS = -Wall -Wextra -I../../../sharedgl/inc/
LDFLAGS = -ldl -lEGL -lGL -lgbm -ldrm -lxcb -lxcb-dri3 -lxcb-present -lxcb-sync -lxshmfence -lm -L../../../sharedgl/build -lX11
INCLUDES = -I/usr/include/libdrm -I./

ifeq ($(DEBUG),1)
GRAMINE_LOG_LEVEL = debug
CFLAGS += -g
else
GRAMINE_LOG_LEVEL = error
CFLAGS += -O3
endif

.PHONY: all
all: minetest.manifest

# Correct manifest generation
minetest.manifest: minetest.manifest.template
	gramine-manifest \
		-Dlog_level=$(GRAMINE_LOG_LEVEL) \
		-Dentrypoint="/udp" \
		-Denclave_size="256M" \
		$< $@
	gramine-manifest-check $@

.PHONY: clean
clean:
	rm -f *.manifest *.sig *.token


