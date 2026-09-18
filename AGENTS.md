# Repository Overview

This repository runs graphics applications in a modified Gramine VM.
`egl-redirect/egl_redirect.so` is preloaded into each application to set up EGL/GBM
and presentation. Gramine forwards the required guest graphics operations to
`qemu/util/sg-listener.c`, which services them using the host graphics stack.

`application -> egl-redirect + Gramine -> QEMU sg-listener -> host graphics`

Games and graphics examples live in `gramine-tdx/CI-Examples/graphics/`.
Relevant Games and graphics examples are
- `glxgears-with-lows`
- `supertuxkart`
- `minetest`
- `quake2`

## Build and Run

Build and enter the Podman environment from the host:

```sh
cd docker
./build.sh
./exec.sh
```

All compilation, installation, and application execution must happen inside the
container. Use each component or game's provided install/setup script; do not
replace it with ad-hoc build commands. Run any required `make` step afterward from
the same example directory to generate its Gramine manifest.

Inside the container, the repository is at `/root/Tirith`. Run examples
from their graphics directory with the provided `run.sh` script or with
`gramine-vm <application>`.
