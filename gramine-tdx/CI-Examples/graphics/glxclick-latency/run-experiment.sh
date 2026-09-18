#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
CLICK_COUNT=100
CLICK_INTERVAL_MS="${CLICK_LATENCY_INTERVAL_MS:-50}"
WINDOW_TIMEOUT_SEC="${CLICK_LATENCY_WINDOW_TIMEOUT_SEC:-30}"
NATIVE_CPU="${CLICK_LATENCY_CPU:-1}"
WRITE_CSV=0
RUN_COUNT=1
count_seen=0
runs_seen=0

usage() {
    echo "usage: $0 [-csv] [-r runs] [click-count]" >&2
    echo "       defaults: 1 run, 100 clicks per runtime per run" >&2
    exit 2
}

while (($#)); do
    case "$1" in
        -csv)
            ((WRITE_CSV == 0)) || usage
            WRITE_CSV=1
            ;;
        -r|--runs)
            ((runs_seen == 0)) || usage
            shift
            (($#)) || usage
            [[ "$1" =~ ^[1-9][0-9]*$ ]] || usage
            RUN_COUNT="$1"
            runs_seen=1
            ;;
        *)
            if [[ "$1" =~ ^[1-9][0-9]*$ ]]; then
                ((count_seen == 0)) || usage
                CLICK_COUNT="$1"
                count_seen=1
            else
                usage
            fi
            ;;
    esac
    shift
done

[[ "$CLICK_INTERVAL_MS" =~ ^[1-9][0-9]*$ ]] || {
    echo "error: CLICK_LATENCY_INTERVAL_MS must be a positive integer" >&2
    exit 2
}
[[ "$NATIVE_CPU" =~ ^[0-9]+$ ]] || {
    echo "error: CLICK_LATENCY_CPU must identify one CPU" >&2
    exit 2
}

for command in xdotool python3 timeout taskset; do
    command -v "$command" >/dev/null 2>&1 || {
        echo "error: required command not found: $command" >&2
        exit 1
    }
done
taskset -c "$NATIVE_CPU" true 2>/dev/null || {
    echo "error: CPU $NATIVE_CPU is unavailable to this container" >&2
    exit 1
}
[[ -w /dev/uinput ]] || {
    echo "error: /dev/uinput is not writable; run inside the privileged Podman environment" >&2
    exit 1
}

make -s -C "$SCRIPT_DIR" >&2

timestamp="$(date +%Y%m%d-%H%M%S)"
work_dir="$(mktemp -d "${TMPDIR:-/tmp}/gramine-click-latency.XXXXXX")"
csv_output=""
if ((WRITE_CSV)); then
    csv_output="$SCRIPT_DIR/results/$timestamp/latency.csv"
fi

child_pid=""
cleanup() {
    if [[ -n "$child_pid" ]] && kill -0 "$child_pid" 2>/dev/null; then
        kill -TERM "$child_pid" 2>/dev/null || true
        wait "$child_pid" 2>/dev/null || true
    fi
    rm -rf -- "$work_dir"
}

run_native() {
    local application_log="$1"
    local inject_log="$2"

    (
        cd "$SCRIPT_DIR"
        env -u LD_PRELOAD \
            NATIVE_INPUT_LATENCY=1 \
            LD_LIBRARY_PATH="/opt/mesa/lib/x86_64-linux-gnu:/lib:/usr/lib:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu" \
            GALLIUM_THREAD=0 \
            GLIBC_TUNABLES="glibc.pthread.rseq=0" \
            __GL_SYNC_TO_VBLANK=0 \
            vblank_mode=0 \
            GBM_BACKEND=dri \
            EGL_DISCRETE=0 \
            EGL_SYSCALL_LOG=0 \
            EGL_IOCTL_LOG=0 \
            EGL_DUMP_PNG=0 \
            MESA_LOG=0 \
            timeout --signal=TERM --kill-after=5 "$experiment_timeout" \
            taskset -c "$NATIVE_CPU" ./glxclick-latency "$CLICK_COUNT"
    ) >"$application_log" 2>&1 &
    child_pid=$!
    run_click_train "Native Click Latency" "$application_log" "$inject_log"
}

run_gramine() {
    local application_log="$1"
    local inject_log="$2"

    (
        cd "$SCRIPT_DIR"
        SG_INPUT_LATENCY=1 \
            GRAMINE_RAM_SIZE="${GRAMINE_RAM_SIZE:-16G}" \
            GRAMINE_CPU_NUM=1 \
            timeout --signal=TERM --kill-after=5 "$experiment_timeout" \
            gramine-vm glxclick-latency "$CLICK_COUNT"
    ) >"$application_log" 2>&1 &
    child_pid=$!
    run_click_train "Gramine Click Latency" "$application_log" "$inject_log"
}
trap cleanup EXIT INT TERM

# Allow startup plus the configured click train and a generous shutdown margin.
experiment_timeout=$((WINDOW_TIMEOUT_SEC + 20 +
    (CLICK_COUNT * CLICK_INTERVAL_MS + 999) / 1000))

run_click_train() {
    local title="$1"
    local application_log="$2"
    local inject_log="$3"
    local deadline=$((SECONDS + WINDOW_TIMEOUT_SEC))
    local window_id=""

    while ((SECONDS < deadline)); do
        window_id="$(xdotool search --onlyvisible --name "^${title}$" \
            2>/dev/null | tail -n 1 || true)"
        [[ -n "$window_id" ]] && break
        if ! kill -0 "$child_pid" 2>/dev/null; then
            echo "error: application exited before creating the '$title' window" >&2
            tail -n 100 "$application_log" >&2
            exit 1
        fi
        sleep 0.05
    done

    if [[ -z "$window_id" ]]; then
        echo "error: timed out waiting for the '$title' window" >&2
        tail -n 100 "$application_log" >&2
        exit 1
    fi

    xdotool windowactivate --sync "$window_id" 2>/dev/null || \
        xdotool windowfocus --sync "$window_id"
    xdotool windowraise "$window_id" 2>/dev/null || true
    local geometry
    local window_width
    local window_height
    geometry="$(xdotool getwindowgeometry --shell "$window_id")"
    window_width="$(awk -F= '$1 == "WIDTH" { print $2 }' <<<"$geometry")"
    window_height="$(awk -F= '$1 == "HEIGHT" { print $2 }' <<<"$geometry")"
    if [[ ! "$window_width" =~ ^[1-9][0-9]*$ ||
          ! "$window_height" =~ ^[1-9][0-9]*$ ]]; then
        echo "error: could not determine '$title' window geometry" >&2
        exit 1
    fi
    xdotool mousemove --window "$window_id" \
        "$((window_width / 2))" "$((window_height / 2))"

    "$SCRIPT_DIR/click_latency" "$CLICK_COUNT" "$CLICK_INTERVAL_MS" \
        >"$inject_log" 2>&1

    set +e
    wait "$child_pid"
    local status=$?
    set -e
    child_pid=""
    if ((status != 0)); then
        echo "error: '$title' exited with status $status" >&2
        tail -n 100 "$application_log" >&2
        exit "$status"
    fi
}

native_logs=()
native_inject_logs=()
gramine_logs=()
gramine_inject_logs=()

for ((round = 1; round <= RUN_COUNT; round++)); do
    native_log="$work_dir/round-${round}-native.log"
    native_inject_log="$work_dir/round-${round}-native-inject.log"
    gramine_log="$work_dir/round-${round}-gramine.log"
    gramine_inject_log="$work_dir/round-${round}-gramine-inject.log"
    native_logs+=("$native_log")
    native_inject_logs+=("$native_inject_log")
    gramine_logs+=("$gramine_log")
    gramine_inject_logs+=("$gramine_inject_log")

    if ((round % 2 == 1)); then
        echo "Round $round/$RUN_COUNT: native then Gramine ($CLICK_COUNT clicks each)" >&2
        run_native "$native_log" "$native_inject_log"
        run_gramine "$gramine_log" "$gramine_inject_log"
    else
        echo "Round $round/$RUN_COUNT: Gramine then native ($CLICK_COUNT clicks each)" >&2
        run_gramine "$gramine_log" "$gramine_inject_log"
        run_native "$native_log" "$native_inject_log"
    fi
done

analysis_args=("$CLICK_COUNT")
for ((round = 0; round < RUN_COUNT; round++)); do
    analysis_args+=(
        --run
        "${native_inject_logs[$round]}"
        "${native_logs[$round]}"
        "${gramine_inject_logs[$round]}"
        "${gramine_logs[$round]}"
    )
done
if ((WRITE_CSV)); then
    analysis_args+=(--csv-output "$csv_output")
fi
"$SCRIPT_DIR/analyze_latency.py" "${analysis_args[@]}"
