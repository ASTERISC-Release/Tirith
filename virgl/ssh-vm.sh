#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
SSH_PORT="${SSH_PORT:-2222}"

export SSH_ASKPASS=/bin/true
export SSH_ASKPASS_REQUIRE=force

ssh_arguments=(
    -o StrictHostKeyChecking=accept-new
    -o UserKnownHostsFile="$SCRIPT_DIR/vm/known_hosts"
    -o PreferredAuthentications=password
    -o NumberOfPasswordPrompts=1
    -o ConnectionAttempts=1
    -o ConnectTimeout=3
    -o ServerAliveInterval=2
    -o ServerAliveCountMax=1
    -p "$SSH_PORT"
)
if [[ "${SSH_TTY:-0}" == 1 ]]; then
    ssh_arguments+=(-tt)
elif [[ -t 0 && -t 1 ]]; then
    ssh_arguments+=(-t)
fi

exec setsid -w ssh "${ssh_arguments[@]}" gamer@127.0.0.1 "$@"
