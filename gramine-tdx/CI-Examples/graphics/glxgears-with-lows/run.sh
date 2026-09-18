#!/bin/bash

unset env

export LD_LIBRARY_PATH="/opt/mesa/lib/x86_64-linux-gnu/:/lib:/usr/lib:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu"
export VK_ICD_FILENAMES="/opt/mesa/share/vulkan/icd.d/mesa_icd.x86_64.json"

export GLIBC_TUNABLES="glibc.pthread.rseq=0"
export GALLIUM_THREAD="0"
export __GL_SYNC_TO_VBLANK=0
export vblank_mode=0

export ALSOFT_DRIVERS="alsa"
export SDL_AUDIODRIVER="dummy"
export PULSE_SERVER="none"
export PIPEWIRE_REMOTE="none"
export GBM_BACKEND="dri"

export EGL_DISCRETE="0"
export EGL_SYSCALL_LOG="0"
export EGL_IOCTL_LOG="0"
export EGL_DUMP_PNG="0"
export MESA_LOG="0"

ARGS="1920 1080"

USE_SHAREDGL="${3:-0}"
SHAREDGL_RUN="/home/gamer/shared/sharedgl/sharedgl-run"

if [[ -z "$1" || -z "$2" ]]; then
    echo Usage: ./q2.sh \<egl-redirect? 0\|1\> \<limit cpu? 0\|1\> [sharedgl? 0\|1\]
    exit -1
fi
LIMIT_CPU="$2"

run_game() {
    if [[ "$USE_SHAREDGL" == "1" ]]; then
        if [[ ! -x "$SHAREDGL_RUN" ]]; then
            echo "SharedGL wrapper not found or not executable: $SHAREDGL_RUN" >&2
            exit 1
        fi
        if [[ "$LIMIT_CPU" == "1" ]]; then
            taskset -c 1 "$SHAREDGL_RUN" "$@"
        else
            "$SHAREDGL_RUN" "$@"
        fi
    elif [[ "$LIMIT_CPU" == "1" ]]; then
        taskset -c 1 "$@"
    else
        "$@"
    fi
}

sleep 2 # Enough time to switch workspace before window opens
if [[ "$1" == "1" ]]; then
    export LD_PRELOAD=/root/Tirith/egl-redirect/egl_redirect.so
fi



run_game ./helloworld $ARGS
