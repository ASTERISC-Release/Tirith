// server_responder.c
#include "shm_proto.h"
#include <SDL2/SDL.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <SDL2/SDL_syswm.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

static shm_region_t *g_shm = NULL;

int create_shm_region(void) {
  shm_unlink(SHM_NAME); // remove stale
  int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0600);
  if (fd < 0) {
    perror("shm_open");
    return -1;
  }
  if (ftruncate(fd, sizeof(shm_region_t)) != 0) {
    perror("ftruncate");
    close(fd);
    return -1;
  }
  void *mem = mmap(NULL, sizeof(shm_region_t), PROT_READ | PROT_WRITE,
                   MAP_SHARED, fd, 0);
  close(fd);
  if (mem == MAP_FAILED) {
    perror("mmap");
    return -1;
  }
  g_shm = (shm_region_t *)mem;

  memset(g_shm, 0, sizeof(*g_shm));
  pthread_mutexattr_t ma;
  pthread_condattr_t ca;
  pthread_mutexattr_init(&ma);
  pthread_mutexattr_setpshared(&ma, PTHREAD_PROCESS_SHARED);
  pthread_mutex_init(&g_shm->mutex, &ma);
  pthread_mutexattr_destroy(&ma);

  pthread_condattr_init(&ca);
  pthread_condattr_setpshared(&ca, PTHREAD_PROCESS_SHARED);
  pthread_cond_init(&g_shm->cond_client, &ca);
  pthread_cond_init(&g_shm->cond_server, &ca);
  pthread_condattr_destroy(&ca);

  g_shm->req.cookie = MAGIC_COOKIE;
  g_shm->resp.cookie = MAGIC_COOKIE;
  return 0;
}

void respond_with_event(uint32_t reqid, const SDL_Event *ev, int status) {
  pthread_mutex_lock(&g_shm->mutex);
  g_shm->resp.resp_id = reqid;
  g_shm->resp.status = status;
  if (status == 0 && ev != NULL) {
    size_t es = sizeof(SDL_Event);
    memcpy(g_shm->resp.event_blob, ev, es);
    g_shm->resp.event_size = (uint32_t)es;
  } else {
    g_shm->resp.event_size = 0;
  }
  pthread_cond_signal(&g_shm->cond_client);
  pthread_mutex_unlock(&g_shm->mutex);
}

// Clear _NET_WM_BYPASS_COMPOSITOR for an SDL window (X11/XWayland).
void clear_bypass_compositor(SDL_Window *win) {
  SDL_SysWMinfo wminfo;
  SDL_VERSION(&wminfo.version);
  if (!SDL_GetWindowWMInfo(win, &wminfo)) {
    fprintf(stderr, "SDL_GetWindowWMInfo failed: %s\n", SDL_GetError());
    return;
  }
  Display *dpy = wminfo.info.x11.display;
  Window xwin = wminfo.info.x11.window;
  if (!dpy || !xwin) {
    fprintf(stderr, "No X11 display/window in WMInfo\n");
    return;
  }

  Atom atom = XInternAtom(dpy, "_NET_WM_BYPASS_COMPOSITOR", False);
  if (atom == None) {
    // atom not defined; nothing to do
    return;
  }

  // Option A: set the property to 0 (CARDINAL)
  //   unsigned long value = 0;
  //   XChangeProperty(dpy, xwin, atom, XA_CARDINAL, 32, PropModeReplace,
  //                   (unsigned char *)&value, 1);

  // Option B (alternative): delete the property entirely
  XDeleteProperty(dpy, xwin, atom);

  XFlush(dpy);
  fprintf(stderr, "Cleared _NET_WM_BYPASS_COMPOSITOR on window 0x%lx\n",
          (unsigned long)xwin);
}

int main(int argc, char **argv) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  if (create_shm_region() != 0)
    return 1;

  // Create server window as needed:
  SDL_Window *win = SDL_CreateWindow(
      "Server Window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480,
      SDL_WINDOW_OPENGL | SDL_RENDERER_PRESENTVSYNC);
  if (win == NULL) {
    fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  //   clear_bypass_compositor(win);
  SDL_Event ev;

  printf("Server window created!\n");

  // Main responder loop:
  for (;;) {
    pthread_mutex_lock(&g_shm->mutex);
    // wait for request
    while (g_shm->req.req_id == 0) {
      pthread_cond_wait(&g_shm->cond_server, &g_shm->mutex);
    }
    // copy request locally and clear slot
    shm_request_t r = g_shm->req;
    g_shm->req.req_id = 0; // mark consumed
    pthread_mutex_unlock(&g_shm->mutex);

    if (r.req_type == REQ_SHUTDOWN) {
      respond_with_event(r.req_id, NULL, 2);
      break;
    } else if (r.req_type == REQ_POLL) {
      if (SDL_PollEvent(&ev)) {
        respond_with_event(r.req_id, &ev, 0);
      } else {
        respond_with_event(r.req_id, NULL, 1); // no event
      }
    } else if (r.req_type == REQ_WAIT) {
      if (SDL_WaitEvent(&ev)) {
        respond_with_event(r.req_id, &ev, 0);
      } else {
        respond_with_event(r.req_id, NULL, 1);
      }
    }
    // server can also process internal tasks here, render, etc.
  }

  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
