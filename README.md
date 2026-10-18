# Tirith Release Artifact (ACM CCS'26)

This repository contains the software artifact for the CCS'26 Paper: "You Shall Not Pass into Ring-0! A User Privacy-Friendly Anti-Cheat Architecture for Personal Computers".

The paper introduces a VM-based video game framework, which we call Tirith.

## System Requirements

Tirith is optimized to execute on AMD x86_64 CPUs and discrete GPUs. Our evaluation system had the following specifications:

```
  OS:     Arch Linux x86_64
  Kernel: Linux 6.19.11-arch1-1
  CPU:    AMD Ryzen 5 5600X (12)
  GPU:    AMD Radeon RX 6950 XT [Discrete]
  Memory: 32 GiB
  Window Manager: Labwc
```

Note that other Intel x86_64 configurations work, but they are largely unoptimized. In particular, the system's functionality has been evaluated on an Intel x86_64 CPU (i9-12900k) and an Intel integrated GPU (Intel UHD-770).

This README assumes you have Arch Linux installed with a Wayland window manager. Development and evaluation used [Labwc](https://labwc.github.io/), but other wayland environments also work.

## System Build Instructions

1. Please install podman (a Linux container environment):

    ```bash
    host> sudo pacman -Syu podman podman-compose
    host> systemctl --user enable --now podman.socket
    ```

2. Build and execute the podman container (~4-5 minutes)
  
    ```bash
    host> cd docker
    host> ./build.sh
    host> ./exec.sh     # opens the container shell
    ```

3. Build and install all the required toolchain inside the container environment 

    - QEMU (~1-2 minutes):

      ```bash
      container> cd Tirith/qemu
      container> ./qemu-install.sh
      ```

    - gramine-tdx (~3-5 minutes): 

      ```bash
      container> cd Tirith/gramine-tdx
      container> ./gramine-install.sh
      ```

    - mesa3d (~3-5 minutes):

      ```bash
      container> cd Tirith/mesa3d
      container> ./mesa-install.sh
      ```

    - OpenGL-EGL Translation Library (<1 minute):

      ```bash
      container> cd Tirith/egl-redirect
      container> make
      ```

## Window Forwarding Setup 

To redirect graphics onto the host environment, we must **disable xhost authentication and enable display forwarding**.

- In a separate terminal shell on the host, please **disable xhost authentication**: 

  ```
  host> cd Tirith/scripts
  host> xhost +
  ```

  *Note: This is just for testing. The system will be updated later to avoid disabling authentication.*

- Then, **enable display forwarding**: 

  - Check your active display with `echo $DISPLAY`:
    * If `:0`, run: 
    
    ```bash
    host> ./host-proxy.sh             # targets /tmp/.X11-unix/X0
    ```

    * If `:1`, run:
    
    ```bash
    host> ./host-proxy-xwayland.sh    # targets /tmp/.X11-unix/X1
    ```


**Important Notes**

- The proxy must be left running for the duration of the follow sections.
- FPS data will in general be output to the terminal every 1 minute, with the exception of Supertuxkart which will print only at the end of the benchmark.
- After the tests are done, please re-enabled xhost authentication 

  ```
  host> xhost -
  ```

#### Troubleshooting
* **`socat E read(6, ...): Connection reset by peer`**:
  * Ensure `xhost +` has been run on the host.
  * Alternatively, run socat directly matching your active socket:
    ```bash
    host> sudo socat VSOCK-LISTEN:6000,fork,reuseaddr UNIX-CONNECT:/tmp/.X11-unix/X1    # substitute with X0 based if DISPLAY=:0
    ```
* **`Authorization required / x11 not available`**:
  * Run `xhost +` on the host to disable X11 restrictions.

## Graphical Benchmark and Games 

Benchmarks like Glxgears and video games are run in the container environment (i.e., `cd docker && ./exec.sh`).

### A. Glxgears 

Compile the benchmark and build the required manifest.

```bash
container> cd Tirith/gramine-tdx/CI-Examples/graphics/glxgears-with-lows
container> make
```

Execute the benchmark; a window with spinning gears will appear and it will print average/1%-low FPS every minute

```bash
container> gramine-vm glxgears
```

### B. Quake 2

Download the game, compile it, and build the required manifest.

```bash
container> cd Tirith/gramine-tdx/CI-Examples/graphics/quake2
container> ./setup-quake.sh
```

Then, execute the game inside a gramine vm:

```bash
container> make
container> gramine-vm quake2
```

During gameplay, you can 

1. Hit **esc** to open the menu, navigate with arrow keys and **enter** to the graphics settings.
2. Increase the graphics options to the maximums (optionally).
3. Close the game with **esc**, navigating to quit and hitting **y**.

### C. Supertuxkart

Download the game, compile it, and build the required manifest.

```bash
container> cd Tirith/gramine-tdx/CI-Examples/graphics/supertuxkart
container> ./install-stx.sh
```

Then, execute the game inside a gramine vm:

```bash
container> make
container> gramine-vm supertuxkart
```

During gameplay, you can 
- Skip through the popups and click the wrench at the bottom.
- Update the graphics level to the maximum and ensure the FPS cap is changed from vsync to 1000.

### D. Minetest


Download the game, compile it, and build the required manifest.

```bash
container> cd Tirith/gramine-tdx/CI-Examples/graphics/minetest
container> ./setup-minetest.sh
```

```bash
container> make
container> gramine-vm minetest
```

To open a new in-game environment (also called "world"),

- Click `New` on the main menu
- Click create in the new screen.
- Click Play Game.

*On subsequent runs, the game should load your created world automatically.*

During gameplay, you can:

- Hit **esc** to open the menu, navigate with arrow keys and **enter** to the graphics settings.
- Increase the graphics options to the maximums.


### E. Team Fortress 2 (Optional/Advanced)

TF2 uses its native 64-bit Linux client and an offline `cp_badlands` bot
workload. Installation requires claiming the free TF2 license with a Steam
account.

First, please install steam and log-in to your account. Then,

```bash
container> cd Tirith/gramine-tdx/CI-Examples/graphics/tf2 
container> ./setup-tf2.sh <STEAM_USERNAME>
```

In a separate terminal shell on the host, please steam proxy:

  ```
  host> cd Tirith/scripts
  host> ./host-steam-proxy.sh
  ```
  *This must be left running while Team Fortress 2 is open with Gramine.*

With the desktop Steam client running, execute the game natively first (to verify): 

```bash
container> ./run-native.sh
```

If the game works correctly, you can execute the game with Tirith:

```bash
container> make
container> make run
```
*Note that the mouse sensitivity is set to 0 for ignoring accidental inputs during benchmarking, you can increase it in the ingame settings *(<esc> -> gear icon)**

See `gramine-tdx/CI-Examples/graphics/tf2/README.md` for alternate Steam
library paths, tracing, current limitations, and the benchmark plan.


## License

Tirith's original code is released under the [GNU General Public License v3.0](LICENSE).

Bundled third-party components keep their own licenses; see the license files in each directory:

- `qemu/`: see [`qemu/LICENSE`](qemu/LICENSE) and [`qemu/COPYING`](qemu/COPYING)
- `gramine-tdx/`: see [`gramine-tdx/LICENSE.txt`](gramine-tdx/LICENSE.txt) (LGPL-3.0)
- `mesa3d/`: see [`mesa3d/docs/license.rst`](mesa3d/docs/license.rst)
- `sharedgl/`: see [`sharedgl/LICENSE`](sharedgl/LICENSE) (MIT)
