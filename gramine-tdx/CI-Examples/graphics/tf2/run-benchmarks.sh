#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd -- "$SCRIPT_DIR/../../../.." && pwd)"
VIRGL_DIR="$REPO_DIR/virgl"
CONTAINER_NAME="${TF2_BENCHMARK_CONTAINER:-ubuntu-gpu-kvm}"
CONTAINER_TF2_DIR=/root/Tirith/gramine-tdx/CI-Examples/graphics/tf2

SAMPLE_COUNT="${TF2_BENCHMARK_SAMPLES:-4}"
INTERVAL_SEC="${TF2_BENCHMARK_INTERVAL_SEC:-60}"
WARMUP_SEC="${TF2_BENCHMARK_WARMUP_SEC:-0}"
CASE_TIMEOUT_SEC="${TF2_BENCHMARK_CASE_TIMEOUT_SEC:-900}"
CASE_ATTEMPTS="${TF2_BENCHMARK_CASE_ATTEMPTS:-2}"
CPU="${TF2_BENCHMARK_CPU:-1}"

read -r -a RUNTIMES <<<"${TF2_BENCHMARK_RUNTIMES:-gramine native-redirect native-unmodified drm-native virgl}"
read -r -a RESOLUTIONS <<<"${TF2_BENCHMARK_RESOLUTIONS:-1280x720 1920x1080 2560x1440}"

RESULTS_PARENT="${TF2_BENCHMARK_RESULTS_DIR:-$SCRIPT_DIR/benchmark-results}"
RUN_ID="${TF2_BENCHMARK_RUN_ID:-$(date +%Y%m%d-%H%M%S)}"
RUN_DIR="$RESULTS_PARENT/$RUN_ID"
RUN_LOG="$RUN_DIR/run.log"
RAW_RESULTS="$RUN_DIR/raw-fps.txt"
LATEST_LINK="$RESULTS_PARENT/latest"

current_pid=""
virgl_started=0

die() {
    echo "error: $*" >&2
    exit 1
}

is_positive_integer() {
    [[ "$1" =~ ^[1-9][0-9]*$ ]]
}

for value in "$SAMPLE_COUNT" "$INTERVAL_SEC" "$CASE_TIMEOUT_SEC" "$CASE_ATTEMPTS"; do
    is_positive_integer "$value" || die "benchmark counts and timeouts must be positive integers"
done
[[ "$WARMUP_SEC" =~ ^[0-9]+$ ]] || die "warmup must be a non-negative integer"
[[ "$CPU" =~ ^[0-9]+$ ]] || die "CPU must be a non-negative integer"

for resolution in "${RESOLUTIONS[@]}"; do
    [[ "$resolution" =~ ^[1-9][0-9]*x[1-9][0-9]*$ ]] ||
        die "invalid resolution: $resolution"
done

for command in grep podman setsid ssh tee timeout; do
    command -v "$command" >/dev/null 2>&1 || die "missing command: $command"
done
[[ -x "$SCRIPT_DIR/run-muvm.sh" ]] || die "missing run-muvm.sh"
[[ -x "$VIRGL_DIR/run-vm.sh" && -x "$VIRGL_DIR/ssh-vm.sh" ]] ||
    die "VirGL VM launchers are unavailable"
podman container exists "$CONTAINER_NAME" || die "missing container: $CONTAINER_NAME"
if [[ "$(podman inspect -f '{{.State.Running}}' "$CONTAINER_NAME")" != true ]]; then
    podman start "$CONTAINER_NAME" >/dev/null
fi

mkdir -p "$RUN_DIR"
ln -sfn "$RUN_DIR" "$LATEST_LINK"
: >"$RUN_LOG"
: >"$RAW_RESULTS"
exec > >(tee -a "$RUN_LOG") 2>&1

fps_lines() {
    grep -E '\[fps\] [0-9.]+s: avg [0-9.]+ FPS, 1% low [0-9.]+ FPS' "$1" || true
}

fps_count() {
    fps_lines "$1" | wc -l
}

stop_current_case() {
    local pid="${current_pid:-}"
    [[ -n "$pid" ]] || return 0

    if kill -0 "$pid" 2>/dev/null; then
        kill -INT -- "-$pid" 2>/dev/null || kill -INT "$pid" 2>/dev/null || true
        for _ in {1..40}; do
            kill -0 "$pid" 2>/dev/null || break
            sleep 0.25
        done
    fi
    if kill -0 "$pid" 2>/dev/null; then
        kill -TERM -- "-$pid" 2>/dev/null || kill -TERM "$pid" 2>/dev/null || true
        for _ in {1..20}; do
            kill -0 "$pid" 2>/dev/null || break
            sleep 0.25
        done
    fi
    if kill -0 "$pid" 2>/dev/null; then
        kill -KILL -- "-$pid" 2>/dev/null || kill -KILL "$pid" 2>/dev/null || true
    fi
    wait "$pid" 2>/dev/null || true
    current_pid=""
}

stop_container_workload() {
    podman exec "$CONTAINER_NAME" bash -lc \
        'had_vm=0
         pgrep -f "[/]root/Tirith/qemu/build/qemu-system-x86_64" >/dev/null 2>&1 && had_vm=1
         pkill -TERM -x tf_linux64 2>/dev/null || true
         pkill -TERM -f "[a]uto-input.sh" 2>/dev/null || true
         pkill -TERM -f "[/]root/Tirith/qemu/build/qemu-system-x86_64" 2>/dev/null || true
         pkill -TERM -x virtiofsd 2>/dev/null || true
         for _ in {1..20}; do
             if ! pgrep -x tf_linux64 >/dev/null 2>&1 &&
                ! pgrep -f "[/]root/Tirith/qemu/build/qemu-system-x86_64" >/dev/null 2>&1 &&
                ! pgrep -x virtiofsd >/dev/null 2>&1; then
                 break
             fi
             sleep 0.25
         done
         pkill -KILL -x tf_linux64 2>/dev/null || true
         pkill -KILL -f "[/]root/Tirith/qemu/build/qemu-system-x86_64" 2>/dev/null || true
         pkill -KILL -x virtiofsd 2>/dev/null || true
         rm -f /tmp/source_engine_*.lock
         for file in /tmp/gramine_vhostfs_*.pid; do
             [ -e "$file" ] || continue
             pid=$(cat "$file" 2>/dev/null || true)
             if [ -z "$pid" ] || ! kill -0 "$pid" 2>/dev/null; then
                 rm -f -- "$file"
             fi
         done
         [ "$had_vm" = 0 ] || sleep 5' \
        >/dev/null 2>&1 || true
}

stop_runtime_workload() {
    case "$1" in
        gramine|native-redirect|native-unmodified)
            stop_container_workload
            ;;
        virgl)
            if virgl_running; then
                "$VIRGL_DIR/ssh-vm.sh" \
                    'pkill -TERM -x tf_linux64 2>/dev/null || true; rm -f /tmp/source_engine_*.lock' \
                    >/dev/null 2>&1 || true
            fi
            ;;
    esac
}

virgl_running() {
    local pid_file="$VIRGL_DIR/vm/ubuntu-virgl-tf2.pid"
    [[ -r "$pid_file" ]] && kill -0 "$(cat "$pid_file")" 2>/dev/null
}

wait_for_virgl_ssh() {
    for _ in {1..90}; do
        if "$VIRGL_DIR/ssh-vm.sh" \
                'mountpoint -q /home/gamer/shared || sudo mount -t 9p -o trans=virtio,version=9p2000.L,msize=104857600,ro repository /home/gamer/shared
                 mountpoint -q /mnt/steamroot || sudo mount -t 9p -o trans=virtio,version=9p2000.L,msize=104857600,ro steamroot /mnt/steamroot
                 mountpoint -q /mnt/steamdata || sudo mount -t 9p -o trans=virtio,version=9p2000.L,msize=104857600,ro steamdata /mnt/steamdata
                 test -x /usr/local/bin/run-tf2 &&
                 test -x /home/gamer/shared/virgl/run-tf2-guest.sh &&
                 mountpoint -q /home/gamer/shared &&
                 mountpoint -q /mnt/steamroot &&
                 mountpoint -q /mnt/steamdata &&
                 DISPLAY=:0 XAUTHORITY=/home/gamer/.Xauthority xset q >/dev/null 2>&1' \
                >/dev/null 2>&1; then
            return 0
        fi
        sleep 1
    done
    return 1
}

stop_virgl_vm() {
    local pid_file="$VIRGL_DIR/vm/ubuntu-virgl-tf2.pid"
    local vm_pid

    virgl_running || return 0
    vm_pid="$(cat "$pid_file")"
    "$VIRGL_DIR/ssh-vm.sh" 'sudo poweroff' >/dev/null 2>&1 || true
    for _ in {1..120}; do
        virgl_running || return 0
        sleep 0.5
    done

    echo "VirGL guest shutdown timed out; terminating QEMU PID $vm_pid" >&2
    kill -TERM "$vm_pid" 2>/dev/null || true
    for _ in {1..20}; do
        kill -0 "$vm_pid" 2>/dev/null || return 0
        sleep 0.25
    done
    kill -KILL "$vm_pid" 2>/dev/null || true
}

prepare_virgl_case() {
    if virgl_running; then
        "$VIRGL_DIR/ssh-vm.sh" 'sudo reboot' >/dev/null 2>&1 || true
        for _ in {1..60}; do
            if ! "$VIRGL_DIR/ssh-vm.sh" true >/dev/null 2>&1; then
                break
            fi
            sleep 0.5
        done
    else
        DETACH=1 "$VIRGL_DIR/run-vm.sh"
        virgl_started=1
    fi
    wait_for_virgl_ssh || return 1

    for _ in {1..60}; do
        if "$VIRGL_DIR/ssh-vm.sh" \
                'DISPLAY=:0 XAUTHORITY=/home/gamer/.Xauthority /usr/local/bin/set-virgl-resolution >/dev/null 2>&1 && DISPLAY=:0 XAUTHORITY=/home/gamer/.Xauthority xrandr --current | grep -q "current 2560 x 1600"' \
                >/dev/null 2>&1; then
            break
        fi
        sleep 1
    done

    "$VIRGL_DIR/ssh-vm.sh" \
        'DISPLAY=:0 XAUTHORITY=/home/gamer/.Xauthority xrandr --current | grep -q "current 2560 x 1600"' ||
        return 1

    local guest_pid steam_pid
    guest_pid="$($VIRGL_DIR/ssh-vm.sh 'cat /proc/sys/kernel/ns_last_pid')"
    steam_pid="$($VIRGL_DIR/ssh-vm.sh 'cat /mnt/steamroot/steam.pid')"
    ((guest_pid < steam_pid)) || {
        echo "VirGL guest PID $guest_pid has passed Steam PID $steam_pid" >&2
        return 1
    }
}

cleanup() {
    stop_current_case
    stop_container_workload
}
trap cleanup EXIT
trap 'exit 130' INT TERM

case_command=()
case_metrics_file=""
build_case_command() {
    local runtime="$1"
    local resolution="$2"
    local common_env=(
        --env "FPS_STATS_INTERVAL_SEC=$INTERVAL_SEC"
        --env "FPS_STATS_WARMUP_SEC=$WARMUP_SEC"
    )

    case "$runtime" in
        gramine)
            case_command=(
                podman exec
                "${common_env[@]}"
                --env GRAMINE_CPU_NUM=1
                --env GRAMINE_RAM_SIZE=16G
                "$CONTAINER_NAME" bash -lc
                "cd '$CONTAINER_TF2_DIR' && exec ./run-vm.sh --res='$resolution'"
            )
            ;;
        native-redirect)
            case_command=(
                podman exec
                "${common_env[@]}"
                --env "TF2_CPU=$CPU"
                "$CONTAINER_NAME" bash -lc
                "cd '$CONTAINER_TF2_DIR' && exec ./run-native.sh 1 1 --res='$resolution'"
            )
            ;;
        native-unmodified)
            case_command=(
                podman exec
                "${common_env[@]}"
                --env "TF2_CPU=$CPU"
                "$CONTAINER_NAME" bash -lc
                "cd '$CONTAINER_TF2_DIR' && exec ./run-native.sh 0 1 --res='$resolution'"
            )
            ;;
        drm-native)
            case_command=(
                env
                "FPS_STATS_INTERVAL_SEC=$INTERVAL_SEC"
                "FPS_STATS_WARMUP_SEC=$WARMUP_SEC"
                "TF2_MUVM_CPU=$CPU"
                "FPS_STATS_OUTPUT=$case_metrics_file"
                "$SCRIPT_DIR/run-muvm.sh" drm-native --res="$resolution"
            )
            ;;
        virgl)
            case_command=(
                env SSH_TTY=1 "$VIRGL_DIR/ssh-vm.sh"
                "FPS_STATS_INTERVAL_SEC=$INTERVAL_SEC FPS_STATS_WARMUP_SEC=$WARMUP_SEC run-tf2 --res='$resolution'"
            )
            ;;
        *)
            die "unknown runtime: $runtime"
            ;;
    esac
}

run_case_attempt() {
    local runtime="$1"
    local resolution="$2"
    local attempt="$3"
    local case_log="$RUN_DIR/${runtime}-${resolution}-attempt${attempt}.log"
    local metrics_source start_time sample_total

    stop_runtime_workload "$runtime"
    : >"$case_log"
    case_metrics_file=""
    metrics_source="$case_log"
    if [[ "$runtime" == drm-native ]]; then
        case_metrics_file="$RUN_DIR/${runtime}-${resolution}-attempt${attempt}-fps.log"
        : >"$case_metrics_file"
        metrics_source="$case_metrics_file"
    fi
    if [[ "$runtime" == virgl ]]; then
        prepare_virgl_case || return 1
    fi
    build_case_command "$runtime" "$resolution"

    echo
    echo "===== $runtime $resolution (attempt $attempt/$CASE_ATTEMPTS) ====="
    printf 'Command:'
    printf ' %q' "${case_command[@]}"
    printf '\n'

    setsid --wait "${case_command[@]}" > >(tee "$case_log") 2>&1 &
    current_pid=$!
    start_time=$SECONDS

    while kill -0 "$current_pid" 2>/dev/null; do
        sample_total="$(fps_count "$metrics_source")"
        if grep -Fq 'error: TF2 never' "$case_log"; then
            echo "Unattended scene setup failed after $sample_total samples" >&2
            stop_current_case
            return 1
        fi
        if ((sample_total >= SAMPLE_COUNT)); then
            break
        fi
        if ((SECONDS - start_time >= CASE_TIMEOUT_SEC)); then
            echo "Timed out after $CASE_TIMEOUT_SEC seconds with $sample_total/$SAMPLE_COUNT samples" >&2
            stop_current_case
            return 1
        fi
        sleep 2
    done

    sample_total="$(fps_count "$metrics_source")"
    if ((sample_total < SAMPLE_COUNT)); then
        echo "Runtime exited after producing $sample_total/$SAMPLE_COUNT samples" >&2
        stop_current_case
        return 1
    fi
    if ! grep -Fq 'positioned unattended TF2 benchmark player' "$case_log"; then
        echo "Runtime produced FPS samples without reaching the benchmark scene" >&2
        stop_current_case
        return 1
    fi

    stop_current_case
    stop_runtime_workload "$runtime"
    {
        echo "===== $runtime $resolution ====="
        fps_lines "$metrics_source" | head -n "$SAMPLE_COUNT"
        echo
    } | tee -a "$RAW_RESULTS"
    return 0
}

echo "TF2 benchmark matrix"
echo "Run directory: $RUN_DIR"
echo "Raw FPS results: $RAW_RESULTS"
echo "Samples per case: $SAMPLE_COUNT"
echo "Collector: ${WARMUP_SEC}s warmup, ${INTERVAL_SEC}s intervals"
echo "Runtimes: ${RUNTIMES[*]}"
echo "Resolutions: ${RESOLUTIONS[*]}"

# An idle 16 GiB VirGL VM would perturb the other baselines. Start it only when
# the matrix reaches the VirGL cases.
stop_virgl_vm

failures=0
for runtime in "${RUNTIMES[@]}"; do
    for resolution in "${RESOLUTIONS[@]}"; do
        success=0
        for ((attempt = 1; attempt <= CASE_ATTEMPTS; attempt++)); do
            if run_case_attempt "$runtime" "$resolution" "$attempt"; then
                success=1
                break
            fi
            echo "Retrying $runtime $resolution" >&2
            stop_runtime_workload "$runtime"
        done
        if ((success == 0)); then
            echo "FAILED: $runtime $resolution" | tee -a "$RAW_RESULTS" >&2
            ((failures += 1))
        fi
    done
done

if ((virgl_started == 1)); then
    stop_virgl_vm
fi

echo
echo "Benchmark matrix complete with $failures failed cases."
echo "Raw FPS results: $RAW_RESULTS"
exit "$failures"
