CC := gcc
CFLAGS := -Wall -Wextra -g -O0 -ldl
LDFLAGS := -g -lX11 -lGL -pthread
FILES := common.c server.c client.c main.c real_x11.c
TESTS := test_handles test_glx_client

all: x11-shm-proxy-threads $(TESTS)

x11-shm-proxy-threads: $(FILES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

libx11shim.so: shim_xlib.c $(FILES)
	$(CC) -shared -fPIC -o $@ shim_xlib.c $(filter-out main.c, $(FILES)) -ldl -pthread

test_handles: $(FILES) test_handles.c
	$(CC) $(CFLAGS) -o $@ $(filter-out main.c, $^) $(LDFLAGS)

test_glx_client: $(FILES) test_glx_client.c
	$(CC) $(CFLAGS) -o $@ $(filter-out main.c, $^) -lGL $(LDFLAGS)

clean:
	rm -f x11-shm-proxy-threads $(TESTS)