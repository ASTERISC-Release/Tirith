#!/usr/bin/env bash
set -euo pipefail

REPO_DIR=/home/gamer/shared
TF2_EXAMPLE="$REPO_DIR/gramine-tdx/CI-Examples/graphics/tf2"
TF2_DIR=/home/gamer/tf2/game
FPS_METRICS_DIR=/home/gamer/tf2
FPS_METRICS_SO="$FPS_METRICS_DIR/fps_metrics.so"
DISPLAY="${DISPLAY:-:0}"
XAUTHORITY="${XAUTHORITY:-/home/gamer/.Xauthority}"
export DISPLAY XAUTHORITY

width=1920
height=1080
bots=0
tf2_arguments=()

usage() {
    echo "usage: run-tf2 [--res=WIDTHxHEIGHT] [--bots] [TF2 arguments...]" >&2
    exit 2
}

for argument in "$@"; do
    case "$argument" in
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

if [[ ! -x "$TF2_DIR/tf_linux64" ]]; then
    echo "error: TF2 has not been copied to the VM disk" >&2
    echo "Run: sync-tf2" >&2
    exit 1
fi
for path in \
    "$TF2_EXAMPLE/prepare-workload.sh" \
    "$TF2_EXAMPLE/auto-input.sh" \
    "$REPO_DIR/egl-redirect/fps_metrics.c"; do
    if [[ ! -e "$path" ]]; then
        echo "error: missing shared repository file: $path" >&2
        exit 1
    fi
done

if [[ ! -f "$FPS_METRICS_SO" || \
      "$REPO_DIR/egl-redirect/fps_metrics.c" -nt "$FPS_METRICS_SO" ]]; then
    gcc -shared -O2 -fPIC -Wall -Wextra -Werror \
        -o "$FPS_METRICS_SO" "$REPO_DIR/egl-redirect/fps_metrics.c" \
        -ldl -pthread
fi

bot_quota=0
if [[ "$bots" == 1 ]]; then
    bot_quota=23
fi
"$TF2_EXAMPLE/prepare-workload.sh" "$TF2_DIR" "$bot_quota"

if [[ ! -r /mnt/steamroot/steam.pid ]]; then
    echo "error: host Steam metadata is not mounted" >&2
    exit 1
fi
read -r steam_pid </mnt/steamroot/steam.pid
if [[ ! "$steam_pid" =~ ^[0-9]+$ ]]; then
    echo "error: invalid host Steam PID: $steam_pid" >&2
    exit 1
fi

steam_home="$(mktemp -d /tmp/tf2-virgl-home.XXXXXX)"
steam_relay_pid=""
tf2_pid=""
auto_input_pid=""
readiness_log=""
cleanup() {
    for pid in "$auto_input_pid" "$tf2_pid" "$steam_relay_pid"; do
        if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
            kill -TERM "$pid" 2>/dev/null || true
            wait "$pid" 2>/dev/null || true
        fi
    done
    if [[ "$steam_home" == /tmp/tf2-virgl-home.* ]]; then
        rm -rf -- "$steam_home"
    fi
    if [[ -n "$readiness_log" ]]; then
        rm -f -- "$readiness_log"
    fi
}
trap cleanup EXIT INT TERM

mkdir -p "$steam_home/.steam" "$steam_home/.local/share"
ln -s /mnt/steamdata "$steam_home/.local/share/Steam"
ln -s /mnt/steamdata "$steam_home/.steam/root"
ln -s /mnt/steamdata "$steam_home/.steam/steam"
ln -s /mnt/steamdata/ubuntu12_32 "$steam_home/.steam/bin"
ln -s /mnt/steamdata/ubuntu12_32 "$steam_home/.steam/bin32"
ln -s /mnt/steamdata/ubuntu12_64 "$steam_home/.steam/bin64"
ln -s /mnt/steamdata/linux32 "$steam_home/.steam/sdk32"
ln -s /mnt/steamdata/linux64 "$steam_home/.steam/sdk64"
for file in exportedsettings.json registry.vdf steam.token; do
    if [[ -f "/mnt/steamroot/$file" ]]; then
        ln -s "/mnt/steamroot/$file" "$steam_home/.steam/$file"
    fi
done
mkfifo "$steam_home/.steam/steam.pipe"

socat TCP4-LISTEN:57343,bind=127.0.0.1,reuseaddr,fork,nodelay \
    TCP4:10.0.2.2:57343,nodelay &
steam_relay_pid=$!
printf '%s\n' "$steam_relay_pid" >"$steam_home/.steam/steam.pid"

export SteamAppId=440
export SteamGameId=440
export SteamNoOverlayUIDrawing=1
export HOME="$steam_home"
export XDG_CONFIG_HOME="$HOME/.config"
export XDG_DATA_HOME="$HOME/.local/share"
export XDG_CACHE_HOME="$HOME/.cache"
export LD_LIBRARY_PATH="$TF2_DIR/bin/linux64:$TF2_DIR/tf/bin/linux64:$TF2_DIR:/lib:/usr/lib:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu"
export LD_PRELOAD="$FPS_METRICS_SO"
export GLIBC_TUNABLES=glibc.pthread.rseq=0
export GALLIUM_THREAD=0
export __GL_SYNC_TO_VBLANK=0
export vblank_mode=0
export SDL_AUDIODRIVER=dummy
export SDL_VIDEODRIVER=x11
export PULSE_SERVER=none
export PIPEWIRE_REMOTE=none
export GBM_BACKEND=dri
export EGL_DISCRETE=0
export EGL_SYSCALL_LOG=0
export EGL_SANITY_FULL=0
export EGL_IOCTL_LOG=0
export EGL_DUMP_PNG=0
export MESA_LOG=0
export FONTCONFIG_FILE=/etc/fonts/fonts.conf
export FONTCONFIG_PATH=/etc/fonts
export FPS_STATS_INTERVAL_SEC="${FPS_STATS_INTERVAL_SEC:-60}"
export FPS_STATS_WARMUP_SEC="${FPS_STATS_WARMUP_SEC:-45}"

cd "$TF2_DIR"
if [[ "${TF2_AUTOINPUT:-1}" != 0 ]]; then
    readiness_log="$TF2_DIR/tf/gramine_ready.log"
    rm -f -- "$readiness_log"
fi

# Steam validates the numeric game PID. A private PID namespace makes the game
# PID deterministic without racing unrelated services in the VM.
sudo --preserve-env unshare --pid --fork --mount-proc \
    setpriv --reuid="$(id -u)" --regid="$(id -g)" --init-groups \
    env LD_LIBRARY_PATH="$LD_LIBRARY_PATH" LD_PRELOAD="$LD_PRELOAD" \
    "$REPO_DIR/virgl/launch-at-pid.sh" "$steam_pid" \
    ./tf_linux64 \
    -game tf \
    -steam \
    -insecure \
    -novid \
    -nobreakpad \
    -nominidumps \
    -nojoy \
    -nosteamcontroller \
    -nohltv \
    -noip \
    -nosound \
    -gl \
    -windowed \
    -w "$width" \
    -h "$height" \
    +exec gramine_workload \
    "${tf2_arguments[@]}" &
tf2_pid=$!

if [[ "${TF2_AUTOINPUT:-1}" != 0 ]]; then
    TF2_AUTOINPUT_READY_FILE="$readiness_log" \
        "$TF2_EXAMPLE/auto-input.sh" "$steam_pid" &
    auto_input_pid=$!
fi

set +e
wait "$tf2_pid"
status=$?
set -e
tf2_pid=""
exit "$status"
