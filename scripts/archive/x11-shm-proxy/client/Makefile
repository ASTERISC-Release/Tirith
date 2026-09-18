CC := gcc
CFLAGS := -g -O0 -fPIC -Iinclude
LDFLAGS := -g -ldl -lrt -pthread
FILES := main.o wrappers.o events.o

all: libx11ipc 

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

libx11ipc: $(FILES)
	$(CC) -shared -fPIC -g -O0 -o libx11ipc.so $(FILES) $(LDFLAGS)

clean: 
	rm -f *.o libx11ipc.so