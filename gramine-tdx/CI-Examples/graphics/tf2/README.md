# Team Fortress 2

This directory contains a native Linux TF2 client integration for
`gramine-vm`. It targets an insecure, LAN-only local server on `cp_badlands`.
Bots are disabled by default; `--bots` adds 23 bots. `-insecure` deliberately
disables VAC; do not use this launch for public matchmaking.

## Install

TF2 is free-to-play, but its client depots require a Steam account to claim the
free license. Anonymous SteamCMD currently reports app 440 as installed after
downloading only the 806 KiB bootstrap depot. It does not grant the roughly
30 GiB shared game depot or the native Linux client depot.

From this directory, run:

```sh
./setup-tf2.sh STEAM_USERNAME
```

SteamCMD prompts for the password and Steam Guard code. Do not put a password
on the command line. SteamCMD may cache local authentication state under the
ignored `.steamcmd/` directory, but no credentials are placed in tracked files.
The download is stored in the ignored `game/` directory. Logging into SteamCMD
with the same account can replace the desktop Steam session. After SteamCMD
finishes, sign back into the desktop Steam client before running TF2.
To confirm the anonymous limitation without credentials, use
`./setup-tf2.sh --anonymous`.

An existing native Linux Steam install can be used without copying it:

```sh
make TF2_DIR="$HOME/.local/share/Steam/steamapps/common/Team Fortress 2"
```

Keep the same `TF2_DIR=...` assignment for the run commands below.

## Establish a native baseline

Start Steam on the host and sign in to the account that owns the free TF2
license. Then launch TF2 directly on the host:

```sh
./run-native.sh 0 1
./run-native.sh 0 1 --bots
./run-native.sh 0 1 --res=1280x720
```

The same baseline also runs inside a project container created from this
branch. Rebuild/recreate an older container, then run:

```sh
podman exec -it ubuntu-gpu-kvm bash -lc \
  'cd /root/Tirith/gramine-tdx/CI-Examples/graphics/tf2 && ./run-native.sh 0 1'
```

The compose configuration shares the host PID namespace and mounts
`$HOME/.steam` plus `$HOME/.local/share/Steam` at their original absolute
paths. Steam's game IPC identifies its client by PID, so mounting only
`steam.pipe` is insufficient. These mounts expose the logged-in Steam session
to the privileged development container; use them only on a trusted host.
The custom `/opt/mesa` installation is mounted from the persistent `MESA_DIR`
cache described in `docker/README.md`; populate it before a Gramine VM run.

All launch paths force TF2's OpenGL backend with `-gl` and force SDL's X11
driver for compatibility with the project's X11/Xwayland proxy. The native
launcher's first argument enables `egl_redirect.so`; its second argument pins
TF2 to CPU 1 to match the VM's single vCPU. Set `TF2_CPU` to select another
host CPU. The gameplay workload is LAN-only, loads `cp_badlands`, and sets
`mp_idledealmethod 0` so a stationary benchmark client is not kicked. Map,
round, and win limits are disabled, and the control points are locked so the
match remains active indefinitely. The current TF2 client still requires an
authenticated desktop Steam runtime to initialize. The launcher passes
Source's `-steam` flag so that it uses that runtime.

TF2's console commands do not dismiss its Welcome and Map Info panels. During
startup, `gramine_workload.cfg` writes TF2's console stream to
`tf/gramine_ready.log`. The launcher waits for `Client reached server_spawn.`,
allows five seconds for the VGUI panel to settle, and advances the Welcome and
Map Info panels. It selects BLUE through the developer console, sends `2` for
Soldier, and confirms class selection from TF2's console output. A final Return
closes any residual Map Info panel before the helper applies the benchmark view
and disables the temporary console log. The helper can be disabled for
interactive debugging:

```sh
TF2_AUTOINPUT=0 ./run-native.sh 0 1
```

After selecting Soldier, the launchers place the player at
`-480 -4512 260.031311` with view angles `0 90 0`. To select another fixed
view, open TF2's developer console at that location and run `getpos`, then split
its printed `setpos` and `setang` values between these two variables:

```sh
TF2_BENCHMARK_POS='-480 -4512 260.031311' \
TF2_BENCHMARK_ANGLES='0 90 0' \
./run-native.sh 0 1
```

The auto-input helper applies the view after selecting the class. These
variables work with the native, Gramine VM, muvm, and VirGL launchers. Set
`TF2_BENCHMARK_VIEW=0` to retain TF2's selected spawn point and camera.

Both launch paths preload the project's low-overhead OpenGL frame-time
collector. After a 45-second startup warmup it prints average FPS and 1% low
FPS for each non-overlapping one-minute window. The timing can be overridden
for debugging:

```sh
FPS_STATS_WARMUP_SEC=0 FPS_STATS_INTERVAL_SEC=10 ./run-native.sh 0 1
```

## Run the cross-runtime benchmark matrix

From the host, run:

```sh
./run-benchmarks.sh
```

The matrix tests Gramine, native execution with and without EGL redirect,
muvm DRM-native, and the VirGL VM. Each runtime runs at 1280x720, 1920x1080,
and 2560x1440 on one CPU or vCPU. It records four 60-second samples per case
and stops the workload as soon as the fourth sample is printed. Progress and
full output are written under `benchmark-results/latest/`; `raw-fps.txt`
contains the FPS lines grouped by runtime and resolution.

## Run with muvm DRM-native

Build the local `muvm` bundle on the host, then launch TF2 from the host rather
than from the project container:

```sh
cd ../../../../muvm
./setup.sh

cd ../gramine-tdx/CI-Examples/graphics/tf2
./run-muvm.sh
./run-muvm.sh venus
./run-muvm.sh --bots
./run-muvm.sh venus --res=1280x720 --bots
```

The launcher uses DRM-native GPU virtualization by default; pass `venus` to use
Venus instead. It binds the microVM to one host CPU and gives the guest one
vCPU. It passes the same OpenGL, Steam, audio, vsync, and FPS-collection
environment used by the native and Gramine launchers. Set `TF2_MUVM_CPU` to
select the host CPU and `TF2_MUVM_MEM` to change guest memory in MiB.
For the Venus path, the launcher preloads a small helper that initializes Xlib's
thread support before Zink starts its presentation threads.

The desktop Steam client listens on host loopback. The launcher uses passt's
host-loopback mapping plus a guest-local relay so TF2 can reach that authenticated
Steam session without running another Steam client inside the microVM. The
launcher also aligns TF2's guest PID with a live host PID because Steam validates
the PID reported through its game IPC. No Gramine X11 or Steam proxy is required
for this launch.

The FPS collector waits 45 seconds, then prints non-overlapping 60-second
intervals. Use the second interval as the stabilized comparison point; the first
can still include map entry and shader-cache activity.

If the game reaches `Spawn Server: cp_badlands` and then reports "The server
requires that you be running Steam", the desktop client is running but is not
logged in. This commonly happens immediately after using the same account with
SteamCMD. Exit TF2, sign back into desktop Steam, and rerun the launcher.

`run-native.sh` uses the caller's existing `HOME` by default, allowing
`libsteam_api.so` to find the running Steam client. To select another mounted
home explicitly, use:

```sh
TF2_NATIVE_HOME=/home/steam-user ./run-native.sh 0 1
```

Running this script in an older or independently-created container normally
cannot work because it lacks the host Steam paths and PID namespace. The
script prints a warning when the Steam pipe is absent instead of treating the
resulting black window as a successful baseline.

## Run with Gramine VM

Inside the project container:

```sh
make
make run
```

Equivalent wrapper invocation:

```sh
GRAMINE_RAM_SIZE=12G GRAMINE_CPU_NUM=1 ./run-vm.sh
GRAMINE_RAM_SIZE=12G GRAMINE_CPU_NUM=1 ./run-vm.sh --bots
GRAMINE_RAM_SIZE=12G GRAMINE_CPU_NUM=1 ./run-vm.sh --res=1280x720
```

All three launchers default to 1920x1080 and accept
`--res=WIDTHxHEIGHT`. The VM wrapper regenerates `tf2.manifest` with the
requested dimensions before launch.

Both native and VM wrappers require `xdotool` unless `TF2_AUTOINPUT=0` is set.
For a VM window with a different title, override the default matcher with
`TF2_WINDOW_PATTERN`.

Before the VM run, leave both required host relays running in separate
terminals. Choose the display relay appropriate for the host session, then
start the TF2-specific Steam relay:

```sh
./scripts/host-proxy-xwayland.sh  # Wayland/Xwayland
# or: ./scripts/host-proxy.sh     # X11

./scripts/host-steam-proxy.sh
```

The display relay listens on VSOCK port 6000. The Steam relay listens on VSOCK
port 57343 and forwards to the logged-in desktop Steam process on
`127.0.0.1:57343`. The latter is required even though the Steam files and
`steam.pipe` are mounted into the container. If it is absent, TF2 reports
`SteamAPI_Init() failed; create pipe failed` and currently terminates with exit
code 139 after loading `server.so`. `run-vm.sh` checks both the desktop Steam
listener and Steam VSOCK relay before booting the VM.

Both launch paths disable the Steam overlay. The VM manifest has a fixed
`LD_PRELOAD` containing only the graphics redirect and FPS collector, while the
native launcher deliberately does not inherit a caller's `LD_PRELOAD`. Both
also set `SteamNoOverlayUIDrawing=1` as a second guard. SteamAPI remains enabled
because the current TF2 client requires its authenticated desktop runtime.

TF2's startup traffic exceeds Linux's default VSOCK stream buffer; the display
proxy scripts set a 16 MiB buffer on the listening socket.

The launchers install `config/gramine_workload.cfg` into the downloaded game and
force `r_lightmap_bicubic 0`; TF2's Linux OpenGL renderer otherwise produces
black lightmapped surfaces when that option is enabled. Source's `-sv_benchmark`
mode is deliberately not used: it runs the server simulation as fast as
possible, which accelerates game time and repeatedly emits bot/class
configuration messages. Bot behavior is not a reproducible graphics benchmark;
record a fixed demo and use Source's `timedemo` command for final measurements.

## Current bring-up status

The host-native baseline and `gramine-vm` path both use the logged-in desktop
Steam runtime. The VM path initializes Mesa OpenGL 4.6, loads the local
`cp_badlands` server, passes the VGUI flow automatically, and leaves the Soldier
idle in the live match. With `--bots`, it also creates 23 bots. It prints average
and 1% low FPS to the launching terminal once per minute. Startup shader
compilation can make the first reported interval unrepresentative; subsequent
intervals cover live gameplay.

The redirector notices SDL's asynchronous 640x480-to-1920x1080 window resize at
swap time and rebuilds its EGL/GBM render targets. QEMU presents the imported
buffer through a centered, fixed-size XRender helper window, including the
required vertical flip. This avoids the former symptom where only a 640x480
upper-left portion of the game appeared in a 1920x1080 window under Xwayland.

TF2 reports unavailable shader combinations while loading both natively and in
the VM. These are benign with the fixed workload setting: the base and generated
OpenGL shader caches link, and the map renders consistently in both runtimes.

TF2 talks to the desktop Steam client over local IPC even for this insecure
offline match. The integration launches with `-steam`; its VM environment uses
the mounted Steam state plus the project's host relay so SteamAPI can
initialize. Gramine's pathname UNIX sockets remain internal to the LibOS, so a
plain mount of `steam.pipe` is not sufficient on its own.
