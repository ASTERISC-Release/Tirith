#!/bin/bash
# SuperTuxKart Build Script (from Source)
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PATCH_DIR="$SCRIPT_DIR/patch_files"
cd "$SCRIPT_DIR"

echo "--- Installing Build Dependencies ---"
sudo apt-get update
sudo apt-get install -y \
    build-essential cmake git subversion pkg-config \
    libbluetooth-dev libsdl2-dev libcurl4-openssl-dev \
    libenet-dev libfreetype6-dev libharfbuzz-dev \
    libjpeg-dev libogg-dev libopenal-dev libpng-dev \
    libssl-dev libvorbis-dev libmbedtls-dev zlib1g-dev

# Create a workspace directory
mkdir -p ./stk-build && cd ./stk-build

echo "--- Cloning Repositories (Code and Assets) ---"
# 1. Clone the game engine code
if [ ! -d "stk-code" ]; then
    git clone https://github.com/supertuxkart/stk-code.git
else
    cd stk-code && git pull && cd ..
fi

# 2. Checkout the assets (Required for the game to run)
# Note: This is ~700MB+ and takes some time.
if [ ! -d "stk-assets" ]; then
    svn checkout https://svn.code.sf.net/p/supertuxkart/code/stk-assets stk-assets
else
    cd stk-assets && svn update && cd ..
fi

echo "--- Building SuperTuxKart ---"
cd stk-code

if compgen -G "$PATCH_DIR/*.patch" > /dev/null; then
    echo "--- Applying SuperTuxKart Source Patches ---"
    for patch_file in "$PATCH_DIR"/*.patch; do
        if git apply --check "$patch_file"; then
            git apply "$patch_file"
        elif git apply --reverse --check "$patch_file"; then
            echo "Patch already applied: $patch_file"
        else
            echo "Failed to apply patch: $patch_file" >&2
            exit 1
        fi
    done
fi

mkdir -p cmake_build
cd cmake_build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release -DNO_SHADERC=on

# Compile using all available CPU cores
make -j$(nproc)

echo "--- Installation Complete ---"
echo "You must first start the game with \`./run.sh 0 1\` to populate a config file"
echo "You also need to open the settings wrench at the bottom of the game window to increase graphics settings and uncap FPS"
echo "Then, you can run the game with make && gramine-vm supertuxkart"
