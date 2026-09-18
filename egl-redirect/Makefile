CC      = gcc

HAS_RETURN_MISMATCH := $(shell sh -c 'printf "%s" "int main(void){return 0;}" | $(CC) -Wreturn-mismatch -Werror -x c - -o /dev/null 2>/dev/null && echo yes || true')

CFLAGS  = -O2 -g -MMD -MP -Wno-error=implicit-function-declaration
LDFLAGS = -ldl -lEGL -lGL -lgbm -ldrm -lxcb -lxcb-dri3 -lxcb-present -lxcb-sync -lxshmfence -lm -Wno-error=implicit-function-declaration 

ifeq ($(HAS_RETURN_MISMATCH),yes)
CFLAGS += -Wno-error=return-mismatch
LDFLAGS += -Wno-error=return-mismatch
endif

INCLUDES = -I/usr/include/libdrm -Ihelpers -Wno-error=implicit-function-declaration

all: egl_redirect.so fps_metrics.so

%.o: %.c
	$(CC) $(CFLAGS) -fPIC $(INCLUDES) -c $< -o $@ $(LDFLAGS)

egl_redirect.so: main.o egl.o gramine.o log.o glx.o xcb.o
	$(CC) -shared -o $@  $^ $(LDFLAGS)

fps_metrics.o: fps_metrics.c
	$(CC) -O2 -fPIC -Wall -Wextra -Werror -c $< -o $@

fps_metrics.so: fps_metrics.o
	$(CC) -shared -o $@ $^ -ldl -pthread

clean:	
	rm -rf *.o *.d *.so

-include $(wildcard *.d)
