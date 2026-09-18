#!/bin/bash

set -e

dir="$(pwd)/build"
configure_args=(
    --disable-werror
    -Dbuildtype=release
)

if [ -d "$dir" ]; then
    echo "build directory exists, reconfiguring for release mode."
    cd "$dir"
else
    echo "build directory DOES NOT exist, configuring for release mode."
    mkdir "$dir"
    cd "$dir"
fi

../configure "${configure_args[@]}"

make -j"$(nproc)" qemu-system-x86_64

if [ $? -eq 0 ]; then
    echo -e "build good, linking...\n------------"
else
    echo "build failed, exiting!"
    exit 1
fi


# Create symlink
echo "sudo needed for symlink..."
sudo ln -sf "$dir/qemu-system-x86_64" /usr/bin/qemu

if [ $? -eq 0 ]; then
    echo "Symlink set!"
else
    echo "Symlink failed!"
    exit 1
fi
