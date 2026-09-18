#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd -- "$SCRIPT_DIR/../../../.." && pwd)"
MUVM_DIR="${MUVM_DIR:-$REPO_DIR/muvm}"
MUVM_BIN="$MUVM_DIR/bin/muvm"
MUVM_GUEST_BIN="$MUVM_DIR/bin/muvm-guest"
VIRGL_BUILD_DIR="$MUVM_DIR/src/virglrenderer/build-amdgpu"
VIRGL_LIB_DIR="$VIRGL_BUILD_DIR/src"
RENDER_SERVER="$VIRGL_BUILD_DIR/server/virgl_render_server"
VIRTIO_ICD="${TF2_MUVM_VIRTIO_ICD:-/usr/share/vulkan/icd.d/virtio_icd.json}"
TF2_DIR="${TF2_DIR:-$SCRIPT_DIR/game}"
FPS_METRICS_SO="$REPO_DIR/egl-redirect/fps_metrics.so"
XINITTHREADS_SO="$SCRIPT_DIR/xinitthreads.so"
TF2_MUVM_CPU="${TF2_MUVM_CPU:-${TF2_CPU:-1}}"
TF2_MUVM_MEM="${TF2_MUVM_MEM:-16384}"
TF2_MUVM_STEAM_HOST_ADDRESS="${TF2_MUVM_STEAM_HOST_ADDRESS:-169.254.1.2}"
steam_pid=""
auto_input_pid=""
readiness_log=""

usage() {
    echo "usage: $0 [drm-native|venus] [--res=WIDTHxHEIGHT] [--bots] [TF2 arguments...]" >&2
    exit 2
}

backend="drm-native"
width=1920
height=1080
bots=0
tf2_arguments=()
for argument in "$@"; do
    case "$argument" in
        drm-native|venus)
            backend="$argument"
            ;;
        --res=*)
            resolution="${argument#--res=}"
            if [[ ! "$resolution" =~ ^[1-9][0-9]*x[1-9][0-9]*$ ]]; then
                echo "error: resolution must be WIDTHxHEIGHT" >&2
                usage
            fi
            width="${resolution%x*}"
            height="${resolution#*x}"
            ;;
        --bots)
            bots=1
            ;;
        -h|--help)
            usage
            ;;
        *)
            tf2_arguments+=("$argument")
            ;;
    esac
done

gpu_mode="drm"
gpu_arguments=()
guest_preload="$FPS_METRICS_SO"
if [[ "$backend" == "venus" ]]; then
    gpu_mode="venus"
    if [[ ! -f "$VIRTIO_ICD" ]]; then
        echo "error: missing Venus ICD $VIRTIO_ICD" >&2
        exit 1
    fi
    make -s -C "$SCRIPT_DIR" xinitthreads.so
    gpu_arguments=(-e "VK_DRIVER_FILES=$VIRTIO_ICD")
    guest_preload="$XINITTHREADS_SO:$guest_preload"
fi

cleanup() {
    if [[ -n "$auto_input_pid" ]]; then
        kill "$auto_input_pid" 2>/dev/null || true
        wait "$auto_input_pid" 2>/dev/null || true
    fi
    if [[ -n "$readiness_log" ]]; then
        rm -f -- "$readiness_log"
    fi
}
trap cleanup EXIT INT TERM

for required_file in \
    "$MUVM_BIN" \
    "$MUVM_GUEST_BIN" \
    "$VIRGL_LIB_DIR/libvirglrenderer.so" \
    "$RENDER_SERVER" \
    "$TF2_DIR/tf_linux64" \
    "$FPS_METRICS_SO"; do
    if [[ ! -e "$required_file" ]]; then
        echo "error: missing $required_file" >&2
        exit 1
    fi
done

for required_command in socat ss; do
    if ! command -v "$required_command" >/dev/null 2>&1; then
        echo "error: $required_command is required on the host" >&2
        exit 1
    fi
done

if [[ ! "$TF2_MUVM_CPU" =~ ^[0-9]+$ ]] ||
        [[ ! -d "/sys/devices/system/cpu/cpu$TF2_MUVM_CPU" ]]; then
    echo "error: TF2_MUVM_CPU must identify one online host CPU" >&2
    exit 1
fi

if ! ss -H -ltn "sport = :57343" 2>/dev/null | grep -q .; then
    cat >&2 <<'EOF'
error: desktop Steam is not listening on TCP port 57343.
Start Steam and sign in before running TF2 under muvm.
EOF
    exit 1
fi

if [[ ! -r "$HOME/.steam/steam.pid" ]]; then
    echo "error: missing $HOME/.steam/steam.pid" >&2
    exit 1
fi
read -r steam_pid <"$HOME/.steam/steam.pid"
if [[ ! "$steam_pid" =~ ^[0-9]+$ ]] || ! kill -0 "$steam_pid" 2>/dev/null; then
    echo "error: desktop Steam PID is not running: $steam_pid" >&2
    exit 1
fi

if [[ "${TF2_AUTOINPUT:-1}" != "0" ]] && ! command -v xdotool >/dev/null 2>&1; then
    echo "error: xdotool is required unless TF2_AUTOINPUT=0 is set" >&2
    exit 1
fi

bot_quota=0
if [[ "$bots" == "1" ]]; then
    bot_quota=23
fi
"$SCRIPT_DIR/prepare-workload.sh" "$TF2_DIR" "$bot_quota"

if [[ "${TF2_AUTOINPUT:-1}" != "0" ]]; then
    readiness_log="$TF2_DIR/tf/gramine_ready.log"
    rm -f -- "$readiness_log"
    TF2_AUTOINPUT_READY_FILE="$readiness_log" \
        "$SCRIPT_DIR/auto-input.sh" &
    auto_input_pid=$!
fi

export PATH="$MUVM_DIR/bin:$PATH"
export LD_LIBRARY_PATH="$VIRGL_LIB_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export RENDER_SERVER_EXEC_PATH="$RENDER_SERVER"

game_ld_library_path="$TF2_DIR/bin/linux64:$TF2_DIR/tf/bin/linux64:$TF2_DIR:/lib:/usr/lib"
metrics_output_arguments=()
if [[ -n "${FPS_STATS_OUTPUT:-}" ]]; then
    metrics_output_arguments=(-e "FPS_STATS_OUTPUT=$FPS_STATS_OUTPUT")
fi

set +e
"$MUVM_BIN" \
    -i \
    -c "$TF2_MUVM_CPU" \
    --mem="$TF2_MUVM_MEM" \
    --gpu-mode="$gpu_mode" \
    "${gpu_arguments[@]}" \
    --passt-args="--map-host-loopback $TF2_MUVM_STEAM_HOST_ADDRESS" \
    -e "TF2_DIR=$TF2_DIR" \
    -e "TF2_MUVM_STEAM_HOST_ADDRESS=$TF2_MUVM_STEAM_HOST_ADDRESS" \
    -e "TF2_MUVM_GAME_PID=$steam_pid" \
    -e "TF2_MUVM_HOST_HOME=$HOME" \
    -e "TF2_WIDTH=$width" \
    -e "TF2_HEIGHT=$height" \
    -e "LD_LIBRARY_PATH=$game_ld_library_path" \
    -e "LD_PRELOAD=$guest_preload" \
    -e SteamAppId=440 \
    -e SteamGameId=440 \
    -e SteamNoOverlayUIDrawing=1 \
    -e HOME \
    -e "XDG_CONFIG_HOME=$HOME/.config" \
    -e "XDG_DATA_HOME=$HOME/.local/share" \
    -e "XDG_CACHE_HOME=$HOME/.cache" \
    -e GLIBC_TUNABLES=glibc.pthread.rseq=0 \
    -e GALLIUM_THREAD=0 \
    -e __GL_SYNC_TO_VBLANK=0 \
    -e vblank_mode=0 \
    -e SDL_AUDIODRIVER=dummy \
    -e SDL_VIDEODRIVER=x11 \
    -e PULSE_SERVER=none \
    -e PIPEWIRE_REMOTE=none \
    -e GBM_BACKEND=dri \
    -e EGL_DISCRETE=0 \
    -e EGL_SYSCALL_LOG=0 \
    -e EGL_SANITY_FULL=0 \
    -e EGL_IOCTL_LOG=0 \
    -e EGL_DUMP_PNG=0 \
    -e MESA_LOG=0 \
    -e FONTCONFIG_FILE=/etc/fonts/fonts.conf \
    -e FONTCONFIG_PATH=/etc/fonts \
    -e "FPS_STATS_INTERVAL_SEC=${FPS_STATS_INTERVAL_SEC:-60}" \
    -e "FPS_STATS_WARMUP_SEC=${FPS_STATS_WARMUP_SEC:-45}" \
    "${metrics_output_arguments[@]}" \
    "$SCRIPT_DIR/run-muvm-guest.sh" \
    "${tf2_arguments[@]}"
status=$?
set -e
exit "$status"
