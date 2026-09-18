#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../../../.." && pwd)"
MUVM_DIR="$REPO_ROOT/muvm"
MUVM_BIN="$MUVM_DIR/bin/muvm"
MUVM_BIN_DIR="$MUVM_DIR/bin"
VIRGL_BUILD_DIR="$MUVM_DIR/src/virglrenderer/build-amdgpu"
VIRGL_LIB_DIR="$VIRGL_BUILD_DIR/src"
RENDER_SERVER="$VIRGL_BUILD_DIR/server/virgl_render_server"
DEPS_LIB_DIR="$MUVM_DIR/deps/lib"
LOG_DIR="$SCRIPT_DIR/glxgears-muvm"
VIRTIO_ICD="/usr/share/vulkan/icd.d/virtio_icd.json"

ARGS=(
  1280
  720
)

usage() {
  echo "usage: $0 [no-limit] [drm-native|venus] [pidstat]" >&2
  exit 2
}

need_file() {
  [[ -e "$1" ]] || {
    echo "error: missing $1" >&2
    exit 1
  }
}

LIMIT_CPU=1
COLLECT_STATS=0
BACKEND="drm-native"

for arg in "$@"; do
  case "$arg" in
    no-limit)
      LIMIT_CPU=0
      ;;
    drm-native|venus)
      BACKEND="$arg"
      ;;
    pidstat)
      COLLECT_STATS=1
      ;;
    *)
      usage
      ;;
  esac
done

CPU_ARG=()
if [[ "$LIMIT_CPU" == "1" ]]; then
  CPU_ARG=(-c 3)
fi

APP_BIN="$SCRIPT_DIR/helloworld"

need_file "$MUVM_BIN"
need_file "$MUVM_BIN_DIR/muvm-guest"
need_file "$VIRGL_LIB_DIR/libvirglrenderer.so"
need_file "$RENDER_SERVER"
need_file "$APP_BIN"
if [[ "$BACKEND" == "venus" ]]; then
  need_file "$VIRTIO_ICD"
fi

export PATH="$MUVM_BIN_DIR:$PATH"
export RENDER_SERVER_EXEC_PATH="$RENDER_SERVER"
if [[ -d "$DEPS_LIB_DIR" ]]; then
  export LD_LIBRARY_PATH="$VIRGL_LIB_DIR:$DEPS_LIB_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
else
  export LD_LIBRARY_PATH="$VIRGL_LIB_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

mkdir -p "$LOG_DIR"
echo "Saving monitor logs to: $LOG_DIR"

MUVM_CMD=(
  "$MUVM_BIN"
  "${CPU_ARG[@]}"
  -e GLIBC_TUNABLES=glibc.pthread.rseq=0
  -e GALLIUM_THREAD=0
  -e __GL_SYNC_TO_VBLANK=0
  -e vblank_mode=0
  -e ALSOFT_DRIVERS=alsa
  -e SDL_AUDIODRIVER=dummy
  -e PULSE_SERVER=none
  -e PIPEWIRE_REMOTE=none
  -e GBM_BACKEND=dri
  -e EGL_DISCRETE=0
  -e EGL_SYSCALL_LOG=0
  -e EGL_IOCTL_LOG=0
  -e EGL_DUMP_PNG=0
  -e MESA_LOG=0
)

if [[ "$BACKEND" == "venus" ]]; then
  MUVM_CMD+=(--gpu-mode=venus -e VK_DRIVER_FILES="$VIRTIO_ICD")
else
  MUVM_CMD+=(--gpu-mode=drm)
fi

MUVM_CMD+=("$APP_BIN" "${ARGS[@]}")

"${MUVM_CMD[@]}" &
MUVMPID=$!

echo "muvm PID: $MUVMPID" | tee "$LOG_DIR/pid.txt"

PIDSTAT_PID=""
VMSTAT_PID=""

if [[ "$COLLECT_STATS" == "1" ]]; then
  pidstat -r -u -h -p "$MUVMPID" 1 >"$LOG_DIR/pidstat.log" &
  PIDSTAT_PID=$!

  vmstat 1 >"$LOG_DIR/vmstat.log" &
  VMSTAT_PID=$!
fi

cleanup() {
  if [[ -n "$PIDSTAT_PID" ]]; then
    kill "$PIDSTAT_PID" 2>/dev/null || true
  fi
  if [[ -n "$VMSTAT_PID" ]]; then
    kill "$VMSTAT_PID" 2>/dev/null || true
  fi
}
trap cleanup EXIT

wait "$MUVMPID"
