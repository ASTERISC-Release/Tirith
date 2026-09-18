CC = gcc
CFLAGS = -Wall -Wextra -fPIC -O2
LDFLAGS = -shared -ldl
LIBNAME = libx11_interceptor.so
SOURCES = interceptor.c

TESTS = test

all: $(LIBNAME) test

$(LIBNAME): $(SOURCES)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

test: test.c $(LIBNAME)
	$(CC) -o $@ test.c -lX11 ./$(LIBNAME)

clean:
	rm -f $(LIBNAME) $(TESTS)

install: $(LIBNAME)
	sudo cp $(LIBNAME) /usr/local/lib/
	sudo ldconfig

.PHONY: all clean install $(TESTS)