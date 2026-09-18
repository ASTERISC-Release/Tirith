#!/bin/bash -e

modpath=$(modinfo -n i915 2>/dev/null) || { echo "[*] $mod not found"; exit 1; }
tmpko="/tmp/i915.ko"

# decompress if .zst, otherwise copy
if [[ "${modpath##*.}" == "zst" ]]; then
    echo "[*] decompressing $modpath -> $tmpko"
    zstd -d --stdout "$modpath" > "$tmpko"
else
    echo "[*] copying $modpath -> $tmpko"
    cp "$modpath" "$tmpko"
fi

# ensure we have a valid ELF and Build ID
if ! readelf -n "$tmpko" >/dev/null 2>&1; then
    echo "[!] $tmpko not a valid ELF; skipping"
    exit 1
fi
echo "[*] $tmpko is a valid ELF"

# runtime base (as root):
BASE=$(sudo cat /sys/module/i915/sections/.text)
echo "module runtime base: $BASE"

# process multiple addresses
ADDRESSES=(
    0xffffffffc08544a3
    0xffffffffc0865231
    0xffffffffc0865785
    0xffffffffc0868736
    0xffffffffc086a035
)

BASE=${BASE#0x}

for ADDR in "${ADDRESSES[@]}"; do
    ADDR=${ADDR#0x}
    OFFSET=$((0x$ADDR - 0x$BASE))
    printf "\naddress: 0x%s -> offset: 0x%x\n" "$ADDR" "$OFFSET"
    sudo addr2line -e "$tmpko" -f -C 0x$(printf "%x" "$OFFSET")
done