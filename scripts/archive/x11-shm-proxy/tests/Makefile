CC := gcc
CFLAGS := -Wall -Wextra -g O0 -fPIC -I../client/include
LDFLAGS := -g -ldl -lrt -pthread

all: test test_advanced test_get_visuals test_match_visuals test_default_visual \
		 test_put_image test_create_free_gc #test_gl #test_put_image_roundtrip 

test_gl: test_gl.c
	$(CC) -g -O0 -o $@ $^ -L../client -lx11ipc -I../client/include -L/home/adil/Projects/gramine-tdx-testing/sharedgl/build -lGL

%: %.c
	$(CC) -g -O0 -o $@ $^ -L../client -lx11ipc -I../client/include

clean: 
	rm -rf test test_advanced test_get_visuals test_match_visuals test_default_visual \
        test_put_image test_put_image_roundtrip test_gl test_create_free_gc
