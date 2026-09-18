#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd -- "$SCRIPT_DIR/.." && pwd)"
VM_DIR="$SCRIPT_DIR/vm"
VM_NAME="ubuntu-virgl-tf2"
SSH_PORT="${SSH_PORT:-2222}"
DETACH="${DETACH:-0}"
export GDK_BACKEND=x11

DISK_IMAGE="$VM_DIR/${VM_NAME}.qcow2"
SEED_IMAGE="$VM_DIR/${VM_NAME}-seed.img"
PIDFILE="$VM_DIR/${VM_NAME}.pid"
SERIAL_LOG="$VM_DIR/${VM_NAME}-serial.log"
QGA_SOCKET="$VM_DIR/${VM_NAME}-qga.sock"
HOST_STEAM_ROOT="${HOST_STEAM_ROOT:-$HOME/.steam}"
HOST_STEAM_DATA="${HOST_STEAM_DATA:-$HOME/.local/share/Steam}"

size_qemu_window() {
    local window_id=""

    command -v xdotool >/dev/null 2>&1 || return 0
    for _ in {1..120}; do
        window_id="$(xdotool search --onlyvisible --name "$VM_NAME" 2>/dev/null | tail -n 1 || true)"
        [[ -n "$window_id" ]] && break
        sleep 0.25
    done
    [[ -n "$window_id" ]] || return 0

    # Keep a normal decorated window while matching the guest desktop size.
    xdotool windowsize --sync "$window_id" 2560 1600 2>/dev/null || true
}

if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
    echo "error: qemu-system-x86_64 is missing" >&2
    echo "On Arch Linux, install: qemu-desktop" >&2
    exit 1
fi
for image in "$DISK_IMAGE" "$SEED_IMAGE"; do
    if [[ ! -f "$image" ]]; then
        echo "error: missing $image; run ./create-vm.sh first" >&2
        exit 1
    fi
done
for directory in "$HOST_STEAM_ROOT" "$HOST_STEAM_DATA"; do
    if [[ ! -d "$directory" ]]; then
        echo "error: missing host Steam directory: $directory" >&2
        exit 1
    fi
done
if [[ ! -e /dev/kvm ]]; then
    echo "error: /dev/kvm is unavailable" >&2
    exit 1
fi

if [[ -f "$PIDFILE" ]]; then
    old_pid="$(cat "$PIDFILE")"
    if kill -0 "$old_pid" 2>/dev/null; then
        echo "$VM_NAME is already running as PID $old_pid" >&2
        exit 1
    fi
    rm -f "$PIDFILE"
fi
if ss -H -ltn "sport = :$SSH_PORT" 2>/dev/null | grep -q .; then
    echo "error: host TCP port $SSH_PORT is already in use" >&2
    exit 1
fi
rm -f "$QGA_SOCKET"
: >"$SERIAL_LOG"

qemu_args=(
    -name "$VM_NAME"
    -machine q35,accel=kvm
    -cpu host
    -smp 1
    -m 16384
    -device virtio-vga-gl
    -display gtk,gl=on,show-cursor=on,full-screen=off,zoom-to-fit=off
    -device qemu-xhci,id=xhci
    -device usb-tablet,bus=xhci.0
    -device usb-kbd,bus=xhci.0
    -device virtio-rng-pci
    -device virtio-serial-pci
    -chardev "socket,path=$QGA_SOCKET,server=on,wait=off,id=qga0"
    -device virtserialport,chardev=qga0,name=org.qemu.guest_agent.0
    -device virtio-net-pci,netdev=net0
    -netdev "user,id=net0,hostfwd=tcp:127.0.0.1:$SSH_PORT-:22"
    -virtfs "local,path=$REPO_DIR,mount_tag=repository,security_model=none,readonly=on,id=repository"
    -virtfs "local,path=$HOST_STEAM_ROOT,mount_tag=steamroot,security_model=none,readonly=on,id=steamroot"
    -virtfs "local,path=$HOST_STEAM_DATA,mount_tag=steamdata,security_model=none,readonly=on,id=steamdata"
    -drive "if=virtio,format=qcow2,file=$DISK_IMAGE"
    -drive "if=virtio,format=raw,readonly=on,file=$SEED_IMAGE"
    -serial "file:$SERIAL_LOG"
    -pidfile "$PIDFILE"
)

if [[ "$DETACH" == 1 ]]; then
    qemu_args+=(-daemonize)
elif [[ "$DETACH" != 0 ]]; then
    echo "error: DETACH must be 0 or 1" >&2
    exit 2
fi

echo "Starting VirGL VM with 1 vCPU and 16 GiB RAM."
echo "SSH: $SCRIPT_DIR/ssh-vm.sh"
echo "Serial log: $SERIAL_LOG"
size_qemu_window &
exec qemu-system-x86_64 "${qemu_args[@]}"
