# Gramine Library OS for Running Games


Tirith was evaluated with these system specifications
```
OS:     Arch Linux x86_64
Kernel: Linux 6.19.11-arch1-1
CPU:    AMD Ryzen 5 5600X (12)
GPU:    AMD Radeon RX 6950 XT [Discrete]
Memory: 32 GiB
```

Other configurations ***may*** work, however are untested and may result in large performance degradation including:
- Any non-AMD CPU
- Any GPU other than a discrete AMD GPU

## Prerequisites

Install podman (a docker alternative) on your Linux environment

### Ubuntu (20.04+, 22.04+, 24.04+)

```
sudo apt update
sudo --now podman.socket
```

### Arch Linux

```
sudo pacman -Syu podman podman-compose
systemctl --user enable --now podman.socket
```


## Getting Started

1. Build and enter the podman container
  `cd docker && ./build.sh`
  `./exec.sh`

2. Build the required toolchain inside the container's shell

  - QEMU:

    ```
    cd <repo>/qemu
    ./qemu-install.sh
    ```

  - gramine-tdx: 

    ```
    cd <repo>/gramine-tdx
    ./gramine-install.sh

  - mesa3d:

    ```
    cd <repo>/mesa3d
    ./mesa-install.sh
    ```

  - OpenGL-EGL Translation Library:

    ```
    cd <repo>/egl-redirect
    make
    ```


## Running the Games and Glxgears

### Initial setup for window forwarding
In a separate terminal outside of the docker environment:

*Run*
`cd <repo>/scripts`
`xhost +`

*and then run*
`./host-proxy-xwayland.sh` **or** `./host-proxy.sh` depending on if your system is using wayland.


### NOTE
1. ***The proxy is required to run the games in Tirith, thus the script must be left running for the duration of the follow sections.***
2. ***The rest of this section must be run in the docker environment.***
3. ***FPS data will in general be output to the terminal every 1 minute, with the exception of Supertuxkart which will print only at the end of the benchmark.***

#### Troubleshooting & Edge Cases
* **`socat E read(6, ...): Connection reset by peer`**:
  * Ensure `xhost +` has been run on the host.
  * Check your active display with `echo $DISPLAY`:
    * If `:0`, run `./host-proxy.sh` (targets `/tmp/.X11-unix/X0`).
    * If `:1`, run `./host-proxy-xwayland.sh` (targets `/tmp/.X11-unix/X1`).
  * Alternatively, run socat directly matching your active socket:
    ```bash
    sudo socat VSOCK-LISTEN:6000,fork,reuseaddr UNIX-CONNECT:/tmp/.X11-unix/X1
    ```
* **`Authorization required / x11 not available`**:
  * Run `xhost +` on the host to disable X11 restrictions.

### Glxgears
*build*

`cd <repo>/gramine-tdk/CI-Examples/graphics/glxgears-with-lows && make`

*run*
`gramine-vm glxgears`


### Quake 2
*build*

`cd <repo>/gramine-tdk/CI-Examples/graphics/quake2 && ./setup-quake.sh`

*in-game setup*
1. Open the game normally with `./run.sh 0 1`.
2. Hit <esc> to open the menu, navigate with arrow keys and <enter> to the graphics settings.
3. Increase the graphics options to the maximums.
4. Close the game with <esc>, navigating to quit and hitting <y> and run the command below to use Tirith.

*run*
`make && gramine-vm quake2`


### Supertuxkart
*build*

`cd <repo>/gramine-tdk/CI-Examples/graphics/supertuxkart && ./install-stx.sh`

*in-game setup*
1. Open the game normally with `./run.sh 0 1`.
2. Skip through the popups and click the wrench at the bottom.
3. Update the graphics level to the maximum and ensure the FPS cap is changed from vsync to 1000.
4. Close the game and run the command below to use Tirith.

*run*
`make && gramine-vm supertuxkart`


### Minetest
*in-game setup*
1. Open the game normally with `./run.sh 0 1`.
2. Hit <esc> to open the menu, navigate with arrow keys and <enter> to the graphics settings.
3. Increase the graphics options to the maximums.
4. Close the game and run the command below to use Tirith.

*build*

`cd <repo>/gramine-tdk/CI-Examples/graphics/minetest && ./setup-minetest.sh`

*run*

`make && gramine-vm minetest`
*in-game setup*
1. Click `New` on the main menu
2. Click create in the new screen.
3. Click Play Game.
**On subsequent runs, the game should load the correct world automatically.**

### Team Fortress 2

TF2 uses its native 64-bit Linux client and an offline `cp_badlands` bot
workload. Installation requires claiming the free TF2 license with a Steam
account; anonymous SteamCMD receives only the small bootstrap depot.

*setup*

`cd <repo>/gramine-tdx/CI-Examples/graphics/tf2 && ./setup-tf2.sh STEAM_USERNAME`

*native/Podman baseline*

With the desktop Steam client running, execute `./run-native.sh` in the project Podman container.

*run*

`make && make run`

See `gramine-tdx/CI-Examples/graphics/tf2/README.md` for alternate Steam
library paths, tracing, current limitations, and the benchmark plan.




## Some Relevant Files and Folders

### Host side request handler
`<repo>/qemu/util/sg-listener.c`
`<repo>/qemu/include/qemu/sg.h`

### OpenGL-EGL Translation Library
`<repo>/egl-redirect` *excluding `latency_preload.c`*

### Gramine modifications
*The majority of modifications exist in:*
`<repo>/gramine-tdx/libos/src/sys/`
`<repo>/gramine-tdx/libos/include/libos_sg.h`
