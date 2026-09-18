#!/usr/bin/env bash
set -euo pipefail

SOURCE=/home/gamer/shared/gramine-tdx/CI-Examples/graphics/tf2/game/
DESTINATION=/home/gamer/tf2/game/

if [[ ! -x "${SOURCE}tf_linux64" ]]; then
    echo "error: TF2 is not installed at ${SOURCE%/}" >&2
    exit 1
fi

mkdir -p "$DESTINATION"
echo "Copying TF2 to the VM disk. This copies approximately 31 GiB on the first run."
rsync -a --delete --info=progress2 "$SOURCE" "$DESTINATION"
echo "TF2 is ready at ${DESTINATION%/}."
