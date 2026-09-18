## Container Setup

NOTE: This assumes a native "Arch Linux" environment currently.

### Getting Started

- Build the container image using `./build.sh`
    - Creates an Ubuntu 24.04 container with QEMU/gramine prerequisites (Check `Containerfile` for details)

- Execute the container using `./exec.sh`
    - Mounts userspace and kernel drivers for KVM and X11
    - Mounts the repository home folder under `/root/<repo-home>`
    - Shares the host PID namespace and mounts the current user's Steam
      runtime so TF2 can use a logged-in desktop Steam client

The Steam mounts expose the current desktop Steam session to this privileged
development container. Only run the project container on a trusted workstation
and with trusted source code. Steam must already be running and logged in on
the host before starting TF2 in the container.

The custom Mesa install is stored persistently at `MESA_DIR` (by default
`$HOME/.cache/Tirith/mesa`) and mounted at `/opt/mesa`. If that cache is
empty but `mesa3d/build` is already configured, install it with:

```sh
podman exec ubuntu-gpu-kvm bash -lc \
  'ninja -C /root/Tirith/mesa3d/build install'
```

You can run `./exec.sh` multiple times to start new terminal windows within the container environment. 

### Installing New Persistent Packages inside the Container

The container does not directly persist new packages installed within the container. 
To add new (persistent) packages to the container, 

- Update the "TOOL_PACKAGES" tag inside the podman-compose.yml file.

```
    args:
        TOOL_PACKAGES: "socat <your-new-package>"
```

- Re-run the execute command `./exec.sh` (closing all existing windows )
