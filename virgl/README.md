# TF2 VirGL VM

Ubuntu 24.04 VM for running the repository's Linux Team Fortress 2 installation
with VirGL. QEMU allocates one vCPU and 16 GiB of RAM. TF2 runs from an SSH
shell, where its FPS reports and console output remain visible. The guest
desktop uses a 2560x1600 display.

## Host setup

On Arch Linux:

```bash
sudo pacman -S --needed qemu-desktop cloud-image-utils
```

Desktop Steam must be running and signed in on the host. The VM mounts the
host Steam metadata read-only and relays Steam's local TCP endpoint so the same
TF2 installation can initialize SteamAPI.

## Create and start the VM

```bash
cd virgl
./create-vm.sh
./run-vm.sh
```

VM images and runtime files are stored under `virgl/vm/` and are ignored by
Git. The guest account is `gamer` with a blank password. XFCE logs in
automatically.

Wait for cloud-init to finish, then connect from another host terminal:

```bash
./ssh-vm.sh
cloud-init status --wait
```

## Stage and run TF2

Copy the existing TF2 installation from the read-only repository share onto
the VM disk once:

```bash
sync-tf2
```

Run the benchmark from the SSH shell. Output, including the minute FPS reports,
stays in that terminal:

```bash
run-tf2
run-tf2 --res=1280x720
run-tf2 --bots --res=1920x1080
```

The default is 1920x1080 with no bots. Additional arguments are passed to TF2.
The launcher loads the repository's TF2 benchmark configuration and handles
the menu input needed to enter the map.

TF2's Steam PID handshake assumes a freshly booted VM. Reboot the VM before a
subsequent benchmark run if `run-tf2` reports that the guest PID counter has
passed the host Steam PID.
