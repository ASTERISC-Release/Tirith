#!/bin/bash
set -u

attempts="${1:-10}"
timeout_seconds="${STK_STARTUP_TIMEOUT:-120}"
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
batch_name="${STK_STRESS_LABEL:-$(date +%Y%m%d-%H%M%S)}"
log_dir="$script_dir/startup-stress-logs/$batch_name"
failures=0
run_command=(env)
active_group=""

stop_active_attempt() {
    trap - INT TERM
    if [[ -n "$active_group" ]]; then
        kill -TERM -- "-$active_group" 2>/dev/null || true
        wait "$active_group" 2>/dev/null || true
    fi
    echo "[startup-stress] interrupted; active VM stopped" >&2
    exit 130
}

trap stop_active_attempt INT TERM

if [[ "${STK_MALLOC_DIAGNOSTICS:-0}" == "1" ]]; then
    run_command+=(MALLOC_CHECK_=3 MALLOC_PERTURB_=165)
fi
if [[ "${STK_DISABLE_SHADER_CACHE:-0}" == "1" ]]; then
    run_command+=(MESA_SHADER_CACHE_DISABLE=true)
fi
run_command+=(gramine-vm supertuxkart-startup)

if ! [[ "$attempts" =~ ^[1-9][0-9]*$ ]]; then
    echo "usage: $0 [positive-attempt-count]" >&2
    exit 2
fi

mkdir -p "$log_dir"
echo "[startup-stress] logs: $log_dir"

for ((attempt = 1; attempt <= attempts; attempt++)); do
    log_file="$log_dir/attempt-$(printf '%02d' "$attempt").log"
    echo "[startup-stress] attempt $attempt/$attempts"

    setsid timeout --signal=INT --kill-after=5s "${timeout_seconds}s" \
        "${run_command[@]}" >"$log_file" 2>&1 &
    active_group=$!
    wait "$active_group"
    status=$?
    active_group=""

    if grep -Eq 'Number of frames: [1-9][0-9]* .*Average FPS:' "$log_file"; then
        echo "[startup-stress] PASS: race rendered and benchmark exited"
        continue
    fi

    failures=$((failures + 1))
    if grep -Eq 'free\(\): invalid pointer|malloc\(\):|corrupted|corruption|double free|VM exited with code (134|139)' "$log_file"; then
        reason="heap corruption or crash"
    elif [[ "$status" -eq 124 ]]; then
        reason="timeout before race"
    else
        reason="exit status $status before race"
    fi

    echo "[startup-stress] FAIL: $reason ($log_file)" >&2
    tail -n 30 "$log_file" >&2
done

echo "[startup-stress] completed: $((attempts - failures)) passed, $failures failed"
((failures == 0))
