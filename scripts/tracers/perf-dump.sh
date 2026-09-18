#!/usr/bin/env bash
set -euo pipefail

# trace-qemu-clflush.sh
# Usage: sudo ./trace-qemu-clflush.sh [duration_seconds]
# Defaults: duration_seconds=12
#
# Produces files in /tmp: probe-record-<PID>-<TS>.data, probe-script-<PID>-<TS>.txt,
#                       sample-record-<PID>-<TS>.data, sample-report-<PID>-<TS>.txt
#
# Behavior:
#  - Tries to autodetect a qemu process; prefer names containing "qemu-system" or "qemu-system-x86"
#  - Tries to find a thread named mmap_listener or vcpu; uses that TID for a thread-specific perf run if found

DURATION=${1:-12}
TS=$(date +%Y%m%d-%H%M%S)
OUTDIR=/tmp/qemu-clflush-traces
mkdir -p "$OUTDIR"

echo "[*] duration: ${DURATION}s, output: $OUTDIR"
if [ "$(id -u)" -ne 0 ]; then
    echo "[!] This script needs sudo to run perf. Re-run with sudo or as root."
    exit 1
fi

# strip i915 symbols to resolve symbols
resolve_i915_symbols() {
    local modules=(i915 xe drm drm_kms_helper drm_gpuvm)
    local mod modpath tmpko buildid b1 b2 dest
    
    # Purge any existing buildid-cache entries for these modules to avoid stale mappings
    perf buildid-cache -P
    
    module_has_symbols() {
        local module_path="$1"
        
        # Require a non-empty regular symbol table; a stripped .ko may still
        # have a build-id but will not help perf resolve internal frames.
        readelf -Ws "$module_path" 2>/dev/null | awk '
            BEGIN { count = 0 }
            $1 ~ /^[0-9]+:$/ && $8 != "" && $8 != "Name" { count++ }
            END { exit(count > 0 ? 0 : 1) }
        '
    }
    
    for mod in "${modules[@]}"; do
        modpath=$(modinfo -n "$mod" 2>/dev/null) || { echo "[*] $mod not found"; continue; }
        tmpko="/tmp/${mod}.ko.$USER"
        
        # decompress if .zst, otherwise copy
        if [[ "${modpath##*.}" == "zst" ]]; then
            echo "[*] decompressing $modpath -> $tmpko"
            zstd -d --stdout "$modpath" > "$tmpko"
        else
            cp "$modpath" "$tmpko"
        fi
        
        # ensure we have a valid ELF and Build ID
        if ! readelf -n "$tmpko" >/dev/null 2>&1; then
            echo "[!] $tmpko not a valid ELF; skipping"
            continue
        fi
        
        if module_has_symbols "$tmpko"; then
            echo "[*] $tmpko contains a symbol table"
        else
            echo "[!] $tmpko appears stripped; perf may still show ${mod}+0x... frames"
        fi
        
        # prefer adding the full .ko to perf's buildid cache
        if perf buildid-cache --add "$tmpko" 2>/dev/null; then
            echo "[*] added $tmpko to perf buildid-cache"
            continue
        else
            echo "[!] perf buildid-cache failed for $tmpko; exit!"
            exit 1
        fi
    done
    
    return 0
}

print_recorded_module_buildids() {
    local perf_data="$1"
    local found=0
    local line
    
    echo "[*] recorded module build IDs in $perf_data:"
    while IFS= read -r line; do
        found=1
        echo "    $line"
    done < <(perf buildid-list -i "$perf_data" 2>/dev/null | grep -E '(/|^).*(i915|xe|drm(_kms_helper|_gpuvm)?)\.ko(\.|$)' || true)
    
    if [ "$found" -eq 0 ]; then
        echo "    [none found for i915/xe/drm modules]"
    fi
}

verify_recorded_module_buildids() {
    local perf_data="$1"
    local modules=(i915 xe drm drm_kms_helper drm_gpuvm)
    local mod tmpko cached_buildid recorded_buildid
    local mismatch=0
    
    for mod in "${modules[@]}"; do
        tmpko="/tmp/${mod}.ko.$USER"
        if [ ! -f "$tmpko" ]; then
            continue
        fi
        
        cached_buildid=$(readelf -n "$tmpko" 2>/dev/null | awk '/Build ID/ {print $3; exit}')
        if [ -z "${cached_buildid:-}" ]; then
            echo "[!] could not read cached build ID for $mod from $tmpko"
            mismatch=1
            continue
        fi
        
        recorded_buildid=$(perf buildid-list -i "$perf_data" 2>/dev/null | awk -v mod="$mod" '
            $0 ~ ("(^|/)" mod "\\.ko(\\.|$)") { print $1; exit }
        ')
        
        if [ -z "${recorded_buildid:-}" ]; then
            echo "[!] $mod was not found in recorded build IDs for $perf_data"
            mismatch=1
            continue
        fi
        
        if [ "$recorded_buildid" = "$cached_buildid" ]; then
            echo "[*] build ID match for $mod: $recorded_buildid"
        else
            echo "[!] build ID mismatch for $mod: perf.data has $recorded_buildid, cached module has $cached_buildid"
            mismatch=1
        fi
    done
    
    return "$mismatch"
}

annotate_module_frames() {
    local input_script="$1"
    local output_script="$2"
    local modules=(i915 xe drm drm_kms_helper drm_gpuvm)
    local mod base line addr modfile resolved symbol source
    declare -A module_file
    declare -A module_base
    
    for mod in "${modules[@]}"; do
        modfile="/tmp/${mod}.ko.$USER"
        if [ -f "$modfile" ]; then
            module_file["$mod"]="$modfile"
            base=$(<"/sys/module/${mod}/sections/.text" 2>/dev/null || true)
            if [ -n "$base" ]; then
                module_base["$mod"]="$base"
            fi
        fi
    done
    
    : > "$output_script"
    while IFS= read -r line; do
        printf '%s\n' "$line" >> "$output_script"
        
        if [[ $line =~ ([[:xdigit:]]{16}).*\(\[([[:alnum:]_]+)\]\) ]]; then
            addr="${BASH_REMATCH[1]}"
            mod="${BASH_REMATCH[2]}"
            
            if [ -z "${module_file[$mod]:-}" ] || [ -z "${module_base[$mod]:-}" ]; then
                continue
            fi
            
            resolved=$(addr2line -e "${module_file[$mod]}" -f -C \
            "$((16#${addr} - 16#${module_base[$mod]#0x}))" 2>/dev/null || true)
            symbol=$(printf '%s\n' "$resolved" | sed -n '1p')
            source=$(printf '%s\n' "$resolved" | sed -n '2p')
            
            if [ -n "$symbol" ] && [ "$symbol" != "??" ]; then
                if [ -n "$source" ] && [ "$source" != "??:0" ]; then
                    printf '        => %s @ %s\n' "$symbol" "$source" >> "$output_script"
                else
                    printf '        => %s\n' "$symbol" >> "$output_script"
                fi
            fi
        fi
    done < "$input_script"
}

# Step 1: find QEMU pid heuristically
find_qemu_pid() {
    # Preference order: explicit qemu-system-x86_64, qemu-system-*, qemu-kvm, kvm/qemu
    for pat in "qemu-system-x86_64" "qemu-system-x86" "qemu-system-" "qemu-kvm" "qemu"; do
        pid=$(pgrep -a -f "$pat" | awk '{print $1; exit}')
        if [ -n "${pid:-}" ]; then
            echo "$pid"
            return 0
        fi
    done
    return 1
}

QPID=$(find_qemu_pid) || true
if [ -z "${QPID:-}" ]; then
    echo "[!!] Could not auto-find a qemu pid. Try running 'pgrep -a qemu' and pass an explicit pid."
    exit 2
fi

echo "[*] picked qemu pid: $QPID"
# cmdline=$(tr '\0' ' ' < /proc/$QPID/cmdline 2>/dev/null || true)
# echo "[*] qemu cmdline: ${cmdline}"

# Step 2: find a good thread (mmap_listener preferred)
find_thread_tid() {
    local pid="$1"
    # try mmap_listener
    for t in "$pid"/task/*; do
        tid=$(basename "$t")
        if [ -f "$pid/task/$tid/comm" ]; then
            name=$(cat "$pid/task/$tid/comm" 2>/dev/null || true)
            case "$name" in
                mmap_listener) echo "$tid"; return 0;;
            esac
        fi
    done
    
    # try vcpu or kvm or qemu threads
    for t in "$pid"/task/*; do
        tid=$(basename "$t")
        name=$(cat "$pid/task/$tid/comm" 2>/dev/null || true)
        if echo "$name" | grep -Eqi 'vcpu|vcpu-thread|vcpu|mmap|listener|qemu'; then
            echo "$tid"; return 0
        fi
    done
    
    # fallback to main pid (thread = pid)
    echo "$pid"
    return 0
}

TID=$(find_thread_tid "$QPID")
echo "[*] chosen TID for thread-specific runs: $TID (comm: $(cat /proc/$QPID/task/$TID/comm 2>/dev/null || true))"

# Step 3: add perf probe for drm_clflush_virt_range (idempotent)
echo "[*] creating perf probe for drm_clflush_virt_range (harmless if already exists)"
set +e
perf probe drm_clflush_virt_range >/dev/null 2>&1
prc=$?
set -e
if [ $prc -eq 0 ]; then
    echo "[*] perf probe created / exists"
else
    echo "[!] perf probe creation returned code $prc (OK if probe already existed)"
fi

# Step 4: resolve i915 symbols
resolve_i915_symbols || echo "[!] Failed to resolve i915 symbols; probe stacks may have many [unknown] frames. Consider building QEMU with -g and/or providing kernel debug symbols."

# Filenames
PROBE_DATA="$OUTDIR/probe-record.data"
PROBE_SCRIPT="$OUTDIR/probe-script.txt"
SAMPLE_DATA="$OUTDIR/sample-record.data"
SAMPLE_REPORT="$OUTDIR/sample-report.txt"

# Step 4: run probe-based perf record (captures when drm_clflush fires)
echo "[*] running perf probe-based capture for pid $QPID (events: probe:drm_clflush_virt_range) -> $PROBE_DATA"
# run in background so we can also run sampling if desired (use different durations if you like)
perf record -e probe:drm_clflush_virt_range -p "$QPID" -g -o "$PROBE_DATA" -- sleep "$DURATION"
echo "[*] probe capture finished; producing human readable stacks to $PROBE_SCRIPT"
perf script -i "$PROBE_DATA" > "$PROBE_SCRIPT" || echo "[!] perf script failed (see $PROBE_DATA)"

# Step 5: run a CPU-sampling perf record (cycles) for the same pid (gives hotspots)
echo "[*] running perf CPU sampling (freq 200Hz) for pid $QPID -> $SAMPLE_DATA"
perf record -F 200 -p "$QPID" -g -o "$SAMPLE_DATA" -- sleep "$DURATION"

echo "[*] generating perf report (dwarf callgraph) to $SAMPLE_REPORT"
perf report --stdio -i "$SAMPLE_DATA" > "$SAMPLE_REPORT" || {
    echo "[!] perf report --call-graph dwarf failed; trying without dwarf..."
    exit 1
}

# Step 6: show summary of results
echo
echo "Done. Outputs in: $OUTDIR"
echo "  * probe data (binary): $PROBE_DATA"
echo "  * probe stacks (text):  $PROBE_SCRIPT"
echo "  * sample data (binary): $SAMPLE_DATA"
echo "  * sample report (text): $SAMPLE_REPORT"
echo
echo "Next steps:"
echo "  - Inspect $PROBE_SCRIPT for exact kernel stacks when drm_clflush_virt_range fired."
echo "  - Inspect $SAMPLE_REPORT for CPU hotspots and callchains."
exit 0
