#!/bin/bash -e

export LD_LIBRARY_PATH=/opt/mesa/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH
export LIBGL_DRIVERS_PATH=/opt/mesa/lib/x86_64-linux-gnu/dri
# export MESA_LOADER_DRIVER_OVERRIDE=iris
export GBM_BACKEND=dri
export VK_ICD_FILENAMES=/opt/mesa/share/vulkan/icd.d/mesa_icd.x86_64.json
export VK_LOADER_DEBUG=all
export MESA_INTEL_PATH_LOG=1

# Run glxgears with the EGL bypass library preloaded
make clean && make
gramine-vm egl
