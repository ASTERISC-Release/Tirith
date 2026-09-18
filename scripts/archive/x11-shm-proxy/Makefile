CC := gcc
CFLAGS := -Wall -Wextra -g O0 -fPIC -Iclient/include
LDFLAGS := -g -ldl -lrt -pthread

all: server client tests

server: server.c
	$(CC) -g -O0 -o server server.c -lX11 -pthread -lrt

client: 
	$(MAKE) -C client

tests: 
	$(MAKE) -C tests

clean:
	rm -f server libx11ipc.so
	$(MAKE) -C client clean
	$(MAKE) -C tests clean

.PHONY: all server client tests clean

