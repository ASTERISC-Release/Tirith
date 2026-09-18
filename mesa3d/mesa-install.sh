#!/bin/bash -e

## Left for legacy purposes
# sudo apt purge -y libllvmspirvlib-20-dev
# sudo apt install -y libllvmspirvlib-18-dev
# export CC=/usr/bin/clang-18
# export CXX=/usr/bin/clang++-18
# export LLVM_CONFIG=/usr/bin/llvm-config-18
# ln -s /usr/bin/clang-18 /usr/local/bin/clang || true

if [ ! -f /usr/local/bin/clang ];
then
    sudo apt update
    sudo apt install -y clang-20 libllvmspirvlib-20-dev
    export CC=/usr/bin/clang-20
    export CXX=/usr/bin/clang++-20
    export LLVM_CONFIG=/usr/bin/llvm-config-20
    unlink /usr/local/bin/clang || true
    ln -s /usr/bin/clang-20 /usr/local/bin/clang || true
fi

if [ -d "./build" ]; then
    pushd ./build
    sudo ninja install -j`nproc`
    popd
else
    sudo meson setup build -Dprefix=/opt/mesa -Dgallium-drivers=auto -Dbuildtype=release -Dvulkan-drivers=intel -Dglvnd=false -Dc_args='-Wno-typedef-redefinition' --wipe
    
    pushd ./build
    sudo ninja install -j`nproc`
    popd
fi