CC      = gcc

HAS_RETURN_MISMATCH := $(shell sh -c 'printf "%s" "int main(void){return 0;}" | $(CC) -Wreturn-mismatch -Werror -x c - -o /dev/null 2>/dev/null && echo yes || true')

CFLAGS  = -g -Wno-error=implicit-function-declaration
LDFLAGS = -ldl -lEGL -lGL -lgbm -ldrm -lxcb -lxcb-dri3 -lxcb-present -lxcb-sync -lxshmfence -lm -Wno-error=implicit-function-declaration 

ifeq ($(HAS_RETURN_MISMATCH),yes)
CFLAGS += -Wno-error=return-mismatch
LDFLAGS += -Wno-error=return-mismatch
endif

INCLUDES = -I/usr/include/libdrm -Ihelpers -Wno-error=implicit-function-declaration

all: egl_hook.so

bypass-egl.o: helpers/bypass-egl.c
	$(CC) -fPIC $(INCLUDES) -c $< -o $@

bypass-egl-check.o: bypass-egl-check.c
	$(CC) -fPIC $(INCLUDES) -c $< -o $@ $(LDFLAGS)

egl_hook.so: bypass-egl-check.o bypass-egl.o
	$(CC) -shared -o $@  $^ $(LDFLAGS)

clean:	
	rm -rf *.o *.so
