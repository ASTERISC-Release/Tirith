CC := gcc
CFLAGS := -Wall -Wextra -g O0 -fPIC
LDFLAGS := -g -ldl -lrt -pthread

all: server client test

server: server_single.c
	$(CC) -g -O0 -o server_single server_single.c -lX11 -pthread -lrt

client: client_single.c
	$(CC) -shared -fPIC -g -O0 -o libx11ipc.so client_single.c $(LDFLAGS)

test: test_x.c
	$(CC) -g -O0 -o test_x test_x.c -lX11

clean:
	rm -f server_single libx11ipc.so test_x

.PHONY: all server client test clean

