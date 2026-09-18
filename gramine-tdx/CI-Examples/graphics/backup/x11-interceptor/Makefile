# ============================================================
# Makefile for building the X11 wrapper LD_PRELOAD library
# ============================================================

# Compiler and flags
CC      := gcc
CFLAGS  := -Wall -Wextra -fPIC -O2
LDFLAGS := -shared -ldl

# Input / output files
SRC     := x11_wrappers.c
OUT     := libx11wrap.so
GEN     := generate_x11_wrappers-libclang.py
HEADER  := /usr/include/X11/Xlib.h

# Include flags for your system
INCLUDES := -I/usr/include -I/usr/include/X11

# ============================================================

.PHONY: all clean regen run test

# Default target: build the .so
all: $(OUT)

# Build the shared library
$(OUT): $(SRC)
	@echo "👉 Building $@"
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(INCLUDES)
	@echo "✅ Built $@"

# Regenerate x11_wrappers.c using libclang
regen:
	@echo "⚙️  Regenerating wrappers from $(HEADER)"
	python3 $(GEN) $(HEADER) \
	    --out $(SRC) \
	    --clang-args "$(INCLUDES) -D_XOPEN_SOURCE=700" -v
	@echo "✅ Generated $(SRC)"

# Run a test (requires xeyes installed)
run: $(OUT)
	@echo "🧪 Running xeyes with LD_PRELOAD..."
	X11WRAP_LOG=1 LD_PRELOAD=$(PWD)/$(OUT) xeyes

# Clean all build artifacts
clean:
	rm -f $(OUT) $(SRC)
	@echo "🧹 Clean complete."

# Quick test with your own small C app (test_x.c)
test: $(OUT)
	@echo "🧪 Running ./test_x with LD_PRELOAD..."
	X11WRAP_LOG=1 LD_PRELOAD=$(PWD)/$(OUT) xclock

