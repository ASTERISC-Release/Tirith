#!/bin/bash

export GALLIUM_THREAD="0"
export GLIBC_TUNABLES="glibc.pthread.rseq=0"
export __GL_SYNC_TO_VBLANK=0
export vblank_mode=0
export ALSOFT_DRIVERS="alsa"
export SDL_AUDIODRIVER="dummy"
export PULSE_SERVER="none"
export PIPEWIRE_REMOTE="none"
export LD_PRELOAD=./../../../../egl-redirect/egl_redirect.so

glxgears
