# Copyright (C) 2026 Gramine contributors
# SPDX-License-Identifier: BSD-3-Clause

TF2_DIR ?= $(CURDIR)/game
TF2_STEAM_HOME ?= $(if $(TF2_NATIVE_HOME),$(TF2_NATIVE_HOME),$(HOME))
TF2_DISPLAY ?= 2:0
TF2_WIDTH ?= 1920
TF2_HEIGHT ?= 1080
FPS_METRICS_SO := $(CURDIR)/../../../../egl-redirect/fps_metrics.so

.PHONY: all
all: tf2.manifest

.PHONY: check-install
check-install:
	@test -x "$(TF2_DIR)/tf_linux64" || { \
		echo "error: $(TF2_DIR)/tf_linux64 is missing" >&2; \
		echo "run ./setup-tf2.sh STEAM_USERNAME, or set TF2_DIR to an existing Linux TF2 install" >&2; \
		exit 1; \
	}
	@test -f "$(TF2_DIR)/tf/bin/linux64/client.so" || { \
		echo "error: the TF2 Linux client depot is incomplete (missing tf/bin/linux64/client.so)" >&2; \
		exit 1; \
	}

$(FPS_METRICS_SO): $(CURDIR)/../../../../egl-redirect/fps_metrics.c \
		$(CURDIR)/../../../../egl-redirect/Makefile
	$(MAKE) -C $(dir $(FPS_METRICS_SO)) fps_metrics.so

xinitthreads.so: xinitthreads.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -Wl,-z,defs -o $@ $< -lX11

.PHONY: FORCE
FORCE:

tf2.manifest: tf2.manifest.template $(FPS_METRICS_SO) FORCE
	gramine-manifest \
		-Dtf2_dir="$(realpath $(TF2_DIR))" \
		-Dsteam_home="$(realpath $(TF2_STEAM_HOME))" \
		-Dintegration_dir="$(CURDIR)" \
		-Ddisplay="$(TF2_DISPLAY)" \
		-Dwidth="$(TF2_WIDTH)" \
		-Dheight="$(TF2_HEIGHT)" \
		$< $@
	gramine-manifest-check $@

.PHONY: run
run: check-install
	GRAMINE_RAM_SIZE="$${GRAMINE_RAM_SIZE:-16G}" \
	GRAMINE_CPU_NUM="$${GRAMINE_CPU_NUM:-1}" \
	./run-vm.sh
.PHONY: clean
clean:
	$(RM) tf2.manifest tf2.manifest.sgx tf2.sig tf2.token xinitthreads.so
