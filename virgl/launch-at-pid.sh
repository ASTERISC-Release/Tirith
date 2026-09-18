#!/usr/bin/env bash
set -euo pipefail

target_pid="$1"
shift

# PID 1 is this supervisor. Fill the private namespace immediately before the
# game fork so unrelated guest services cannot consume Steam's expected PID.
while :; do
    /usr/bin/true &
    probe_pid=$!
    wait "$probe_pid"
    if ((probe_pid == target_pid - 1)); then
        break
    fi
    if ((probe_pid >= target_pid)); then
        echo "error: private PID namespace passed target PID $target_pid" >&2
        exit 1
    fi
done

"$@" &
game_pid=$!
if ((game_pid != target_pid)); then
    echo "error: expected game PID $target_pid, got $game_pid" >&2
    exit 1
fi

set +e
wait "$game_pid"
status=$?
set -e
exit "$status"
