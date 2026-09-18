/* main.c --- client that forwards X calls to the server via single shared memory region ---
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <fcntl.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "common.h"

/* runtime pointers */
shared_region_t *g = NULL;
pid_t mypid = 0;
uint32_t seq_counter = 1;

extern Display* localDisplayPointers[];

/* map shared memory in constructor */
static void __attribute__((constructor)) client_init(void) {
    mypid = getpid();

    /* open with read-write so we can write req flags & rpc_slot */
    int fd = shm_open(SHM_NAME, O_RDWR, 0);
    if (fd < 0) {
        /* server may not be running; graceful fallback */
        fprintf(stderr, "[ipc-client] cannot open shared region %s: %s (Exiting)\n",
                SHM_NAME, strerror(errno));
        _exit(1);
    }

    void *p = mmap(NULL, sizeof(*g), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
        /* if mmap fails, show clear diagnostics and fallback */
        fprintf(stderr, "[ipc-client] mmap failed for %s: %s (Exiting)\n", SHM_NAME, strerror(errno));
        close(fd);
        _exit(1);
    }

    for (int i = 0; i < NUM_DISPLAYS; i++) {
        localDisplayPointers[i] = NULL;
    }

    /* success */
    g = (shared_region_t*)p;
    fprintf(stderr, "[ipc-client] init pid=%d shared=%s ptr=%p\n", mypid, SHM_NAME, (void*)g);
}

/* destructor: unmap */
static void __attribute__((destructor)) client_fini(void) {
    if (g) munmap((void*)g, sizeof(*g));
}

/* debug constructor */
__attribute__((constructor))
static void client_debug_ctor(void) {
    fprintf(stderr, "[ipc-client] debug ctor pid=%d\n", getpid());
}