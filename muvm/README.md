# Portable muvm Bundle

This directory builds a local `muvm` and a local `virglrenderer` with both of the useful AMDGPU paths:

- `venus`
- `drm-native`

## Layout

- `setup.sh`: installs build/runtime dependencies, downloads the latest `muvm` release, clones `virglrenderer`, and builds everything
- `run.sh`: runs an application with `venus` or `drm-native`
- `src/`: unpacked `muvm` release and cloned `virglrenderer` source after setup
- `bin/`: local `muvm` and `muvm-guest`

## Usage

Run setup first:

```sh
./portable/setup.sh
```

Then run applications:

```sh
./portable/run.sh glxgears venus
./portable/run.sh glxgears drm-native
./portable/run.sh glxinfo venus -B
./portable/run.sh glxinfo drm-native -B
```

## Notes

- `setup.sh` detects the distro from `/etc/os-release`.
- `setup.sh` downloads the latest official `muvm` release tarball from GitHub instead of tracking the moving `main` branch.
- On Arch, it installs the required packages directly from `pacman`, including `libkrun`.
- On Ubuntu, it installs the common userspace/build dependencies, uses `rustup` if the system Rust is too old for the selected `muvm` release, downloads a prebuilt `libkrunfw`, and builds a local `libkrun` into `deps/`.
- `run.sh` automatically forces the virtio Vulkan ICD for the `venus` backend when it can find `virtio_icd.json`.
- On Ubuntu-like systems, `run.sh` treats `drm-native` as a compatibility alias for `venus` by default because the stock Mesa DRM native context path is unstable in this container setup. Set `MUVM_UBUNTU_DRM_MODE=native` to force the real `--gpu-mode=drm` path anyway.
**Because of this, it is recommended to only use this system on Arch**
