#!/usr/bin/env bash
set -euo pipefail

TF2_DIR="${TF2_DIR:?TF2_DIR is required}"
STEAM_HOST_ADDRESS="${TF2_MUVM_STEAM_HOST_ADDRESS:-169.254.1.2}"
HOST_HOME="${TF2_MUVM_HOST_HOME:?TF2_MUVM_HOST_HOME is required}"
GAME_PID_TARGET="${TF2_MUVM_GAME_PID:?TF2_MUVM_GAME_PID is required}"
TF2_WIDTH="${TF2_WIDTH:-1920}"
TF2_HEIGHT="${TF2_HEIGHT:-1080}"
STEAM_HOME="$(mktemp -d /tmp/tf2-muvm-home.XXXXXX)"
steam_relay_pid=""
tf2_pid=""

cleanup() {
    if [[ -n "$tf2_pid" ]] && kill -0 "$tf2_pid" 2>/dev/null; then
        kill -TERM "$tf2_pid" 2>/dev/null || true
        wait "$tf2_pid" 2>/dev/null || true
    fi

    if [[ -n "$steam_relay_pid" ]] && kill -0 "$steam_relay_pid" 2>/dev/null; then
        kill -TERM "$steam_relay_pid" 2>/dev/null || true
        wait "$steam_relay_pid" 2>/dev/null || true
    fi

    if [[ "$STEAM_HOME" == /tmp/tf2-muvm-home.* ]]; then
        rm -rf -- "$STEAM_HOME"
    fi
}
trap cleanup EXIT INT TERM

# Steam listens only on the host loopback interface.  passt maps that interface
# to STEAM_HOST_ADDRESS; this final hop preserves the localhost endpoint that
# steamclient.so expects inside the microVM.
(
    exec -a steam socat \
        TCP4-LISTEN:57343,bind=127.0.0.1,reuseaddr,fork,nodelay \
        "TCP4:$STEAM_HOST_ADDRESS:57343,nodelay"
) &
steam_relay_pid=$!

# steamclient.so first checks ~/.steam/steam.pid before connecting to Steam's
# local TCP endpoint.  The desktop Steam PID belongs to the host PID namespace,
# so expose a guest-local Steam view whose PID names the relay above.  All other
# Steam state remains shared directly with the host.
mkdir -p "$STEAM_HOME/.steam" "$STEAM_HOME/.local/share"
ln -s "$HOST_HOME/.local/share/Steam" "$STEAM_HOME/.local/share/Steam"
for steam_entry in \
    bin bin32 bin64 root sdk32 sdk64 steam \
    exportedsettings.json registry.vdf steam.pipe steam.token; do
    if [[ -e "$HOST_HOME/.steam/$steam_entry" || \
            -L "$HOST_HOME/.steam/$steam_entry" ]]; then
        ln -s "$HOST_HOME/.steam/$steam_entry" \
            "$STEAM_HOME/.steam/$steam_entry"
    fi
done
printf '%s\n' "$steam_relay_pid" >"$STEAM_HOME/.steam/steam.pid"

cd "$TF2_DIR"
# Desktop Steam validates the PID reported by the game in the host PID
# namespace.  Advance this short-lived microVM's PID allocator so TF2 has the
# same numeric PID as the long-lived desktop Steam process on the host.
while :; do
    /usr/bin/true &
    probe_pid=$!
    wait "$probe_pid"

    if (( probe_pid == GAME_PID_TARGET - 1 )); then
        break
    fi
    if (( probe_pid >= GAME_PID_TARGET )); then
        echo "error: could not reserve guest TF2 PID $GAME_PID_TARGET" >&2
        exit 1
    fi
done

HOME="$STEAM_HOME" ./tf_linux64 \
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
    -w "$TF2_WIDTH" \
    -h "$TF2_HEIGHT" \
    +exec gramine_workload \
    "$@" &
tf2_pid=$!
if [[ "$tf2_pid" != "$GAME_PID_TARGET" ]]; then
    echo "error: expected TF2 PID $GAME_PID_TARGET, got $tf2_pid" >&2
    exit 1
fi

set +e
wait "$tf2_pid"
status=$?
set -e
tf2_pid=""
exit "$status"
