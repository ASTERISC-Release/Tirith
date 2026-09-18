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
all: egl_hook.so egl.manifest glxg

bypass-egl.o: bypass-egl.c
	$(CC) -fPIC $(INCLUDES) -c $< -o $@

glxg: glxg.o
	$(CC) glxg.o -o glxg $(LDFLAGS)

glxg.o: glxgears.c
	$(CC) -fPIC $(INCLUDES) -g -c $< -o $@

bypass-egl-helper.o: bypass-egl-helper.c
	$(CC) -fPIC $(INCLUDES) -c $< -o $@ $(LDFLAGS)

egl_hook.so: bypass-egl-helper.o bypass-egl.o
	$(CC) -shared -o $@  $^ $(LDFLAGS)

# Correct manifest generation
egl.manifest: egl.manifest.template egl_hook.so
	gramine-manifest \
		-Dlog_level=$(GRAMINE_LOG_LEVEL) \
		-Dentrypoint="/egl-exp" \
		-Denclave_size="256M" \
		$< $@
	gramine-manifest-check $@

.PHONY: clean
clean:
	rm -f *.manifest *.sig *.token egl-exp egl-exp.o egl_hook.so bypass-egl-helper.o bypass-egl.o glxg glxg.o


