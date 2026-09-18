#!/usr/bin/env bash
# cleanup_single.sh - remove POSIX shm and named semaphores used by the single-client x11ipc example
#
# Usage:
#   ./cleanup_single.sh         # dry-run (shows what would be removed)
#   ./cleanup_single.sh -y      # actually remove the objects
#   ./cleanup_single.sh -y --all  # remove exact objects and any /dev/shm entries containing 'x11ipc'
#
set -u
DRY_RUN=1
DO_ALL=0

SHM_NAMES=("/x11ipc_rpc_shm" "/x11ipc_events_shm")
SEM_NAMES=("/x11ipc_req" "/x11ipc_resp" "/x11ipc_events_sem" "/x11ipc_events_mtx")

usage() {
  cat <<EOF
cleanup_single.sh - remove /dev/shm objects created by the single-client x11ipc example

Usage:
  $0 [ -y | --yes ] [ --all ]
Options:
  -y, --yes     Actually unlink/remove objects. Without this flag the script only prints what it would remove.
  --all         Also attempt to remove any /dev/shm entries containing substring 'x11ipc' (aggressive).
  -h, --help    Show this help.

Examples:
  $0            # show what would be removed (dry-run)
  $0 -y         # remove the known objects
  $0 -y --all   # remove known objects + any /dev/shm/*x11ipc* entries
EOF
}

# parse args
while [[ $# -gt 0 ]]; do
  case "$1" in
  -y | --yes)
    DRY_RUN=0
    shift
    ;;
  --all)
    DO_ALL=1
    shift
    ;;
  -h | --help)
    usage
    exit 0
    ;;
  *)
    echo "Unknown arg: $1"
    usage
    exit 1
    ;;
  esac
done

run_cmd() {
  if [[ $DRY_RUN -eq 1 ]]; then
    echo "[dry-run] $*"
  else
    echo "[exec] $*"
    eval "$@"
    rc=$?
    if [[ $rc -ne 0 ]]; then
      echo "  (note) command exited with status $rc"
    fi
  fi
}

echo "=== x11ipc cleanup utility ==="
if [[ $DRY_RUN -eq 1 ]]; then
  echo "Mode: dry-run. Use -y to actually remove objects."
else
  echo "Mode: destructive. Objects will be removed."
fi
echo

echo "Known POSIX named objects to unlink (shm + sem):"
for s in "${SHM_NAMES[@]}"; do
  echo "  shm:  $s    -> /dev/shm/$(basename "$s")"
done
for s in "${SEM_NAMES[@]}"; do
  echo "  sem:  $s    -> /dev/shm/sem.$(echo "$s" | sed 's|/||')"
done
echo

# Remove shared memory objects
for s in "${SHM_NAMES[@]}"; do
  devpath="/dev/shm/$(basename "$s")"
  echo "Processing shm object: $s (file $devpath)"
  if [[ -e "$devpath" ]]; then
    run_cmd rm -f -- "$devpath"
  else
    echo "  not present as file: $devpath"
  fi

  # Try to unlink POSIX shm name via python os.unlink(name)
  if command -v python3 >/dev/null 2>&1; then
    if [[ $DRY_RUN -eq 1 ]]; then
      echo "[dry-run] python3 -c \"import os,sys; os.unlink(sys.argv[1])\" \"$s\"  # attempt shm_unlink"
    else
      python3 -c "import os,sys
try:
    os.unlink(sys.argv[1])
    print('python: shm_unlink(%s) ok' % sys.argv[1])
except Exception as e:
    print('python: shm_unlink(%s) ->' % sys.argv[1], e)
" "$s" || true
    fi
  else
    echo "  python3 not found; skipping shm_unlink attempt"
  fi
  echo
done

# Remove named semaphores
for sem in "${SEM_NAMES[@]}"; do
  devpath="/dev/shm/sem.$(echo "$sem" | sed 's|/||')"
  echo "Processing semaphore: $sem (file $devpath)"

  # Try sem_unlink via python ctypes calling libc.sem_unlink
  if command -v python3 >/dev/null 2>&1; then
    if [[ $DRY_RUN -eq 1 ]]; then
      echo "[dry-run] python3 -c \"import ctypes,sys; print('would call sem_unlink(%s)'%sys.argv[1])\" \"$sem\""
    else
      # call libc.sem_unlink with the semaphore name; ignore errors
      python3 -c "import ctypes,sys
try:
    libc = ctypes.CDLL('libc.so.6')
    libc.sem_unlink.argtypes = [ctypes.c_char_p]
    libc.sem_unlink.restype = ctypes.c_int
    name = sys.argv[1].encode('utf-8')
    res = libc.sem_unlink(name)
    if res == 0:
        print('python: sem_unlink(%s) ok' % sys.argv[1])
    else:
        print('python: sem_unlink(%s) returned %d' % (sys.argv[1], res))
except Exception as e:
    print('python: sem_unlink failed:', e)
" "$sem" || true
    fi
  else
    echo "  python3 not found; skipping sem_unlink attempt"
  fi

  # Also attempt to remove /dev/shm/sem.<name> file
  if [[ -e "$devpath" ]]; then
    run_cmd rm -f -- "$devpath"
  else
    echo "  not present as file: $devpath"
  fi
  echo
done

if [[ $DO_ALL -eq 1 ]]; then
  echo "Aggressive mode: removing any /dev/shm entries containing 'x11ipc' (use with caution)."
  matches=($(ls /dev/shm 2>/dev/null | grep -i 'x11ipc' || true))
  if [[ ${#matches[@]} -eq 0 ]]; then
    echo "  No matches for 'x11ipc' in /dev/shm."
  else
    echo "Found the following matches:"
    for m in "${matches[@]}"; do
      echo "  /dev/shm/$m"
    done
    echo
    if [[ $DRY_RUN -eq 1 ]]; then
      echo "[dry-run] nothing will be deleted. Re-run with -y to delete."
    else
      for m in "${matches[@]}"; do
        echo "Removing /dev/shm/$m"
        rm -f "/dev/shm/$m" || true
      done
    fi
  fi
else
  echo "Aggressive mode not enabled. Use --all to remove additional /dev/shm entries matching 'x11ipc'."
fi

echo
echo "Done. If you still see semaphore or shm objects in /dev/shm, check permissions and consider running with sudo."
