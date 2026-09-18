#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
VM_DIR="$SCRIPT_DIR/vm"
VM_NAME="ubuntu-virgl-tf2"
UBUNTU_RELEASE="noble"
UBUNTU_IMAGE_URL="https://cloud-images.ubuntu.com/${UBUNTU_RELEASE}/current/${UBUNTU_RELEASE}-server-cloudimg-amd64.img"
DISK_SIZE="80G"

BASE_IMAGE="$VM_DIR/${UBUNTU_RELEASE}-server-cloudimg-amd64.img"
DISK_IMAGE="$VM_DIR/${VM_NAME}.qcow2"
SEED_IMAGE="$VM_DIR/${VM_NAME}-seed.img"
CLOUD_INIT_DIR="$VM_DIR/cloud-init"

for command in curl qemu-img cloud-localds; do
    if ! command -v "$command" >/dev/null 2>&1; then
        echo "error: missing host command: $command" >&2
        echo "On Arch Linux, install: qemu-desktop cloud-image-utils" >&2
        exit 1
    fi
done

mkdir -p "$VM_DIR" "$CLOUD_INIT_DIR"

if [[ ! -f "$BASE_IMAGE" ]]; then
    echo "Downloading Ubuntu 24.04 cloud image..."
    curl --fail --location --continue-at - \
        --output "$BASE_IMAGE" "$UBUNTU_IMAGE_URL"
fi

if [[ ! -f "$DISK_IMAGE" ]]; then
    echo "Creating $DISK_IMAGE ($DISK_SIZE virtual size)..."
    qemu-img convert -O qcow2 "$BASE_IMAGE" "$DISK_IMAGE"
    qemu-img resize "$DISK_IMAGE" "$DISK_SIZE"
else
    echo "Keeping existing VM disk: $DISK_IMAGE"
fi

cat >"$CLOUD_INIT_DIR/meta-data" <<EOF
instance-id: $VM_NAME
local-hostname: $VM_NAME
EOF

cat >"$CLOUD_INIT_DIR/user-data" <<'EOF'
#cloud-config
hostname: ubuntu-virgl-tf2
timezone: America/Phoenix
ssh_pwauth: true
disable_root: true
package_update: true
package_upgrade: false

users:
  - default
  - name: gamer
    gecos: VirGL TF2
    groups: sudo,adm,video,render
    shell: /bin/bash
    lock_passwd: false
    sudo: ALL=(ALL) NOPASSWD:ALL

packages:
  - qemu-guest-agent
  - openssh-server
  - ca-certificates
  - rsync
  - socat
  - build-essential
  - xserver-xorg
  - xfce4
  - lightdm
  - dbus-x11
  - xdotool
  - mesa-utils
  - libegl-dev
  - libgl-dev
  - libgl1
  - libglx-mesa0
  - libfontconfig1
  - libfreetype6
  - libx11-6
  - libxext6
  - libxfixes3
  - libxi6
  - libxrandr2
  - libxrender1
  - libxtst6
  - libsdl2-2.0-0
  - libopenal1

write_files:
  - path: /etc/lightdm/lightdm.conf.d/50-autologin.conf
    owner: root:root
    permissions: '0644'
    content: |
      [Seat:*]
      autologin-user=gamer
      autologin-user-timeout=0
      user-session=xfce
  - path: /etc/ssh/sshd_config.d/60-empty-password.conf
    owner: root:root
    permissions: '0644'
    content: |
      PasswordAuthentication yes
      PermitEmptyPasswords yes
      UsePAM yes
  - path: /home/gamer/.xsession
    owner: gamer:gamer
    permissions: '0644'
    defer: true
    content: |
      exec startxfce4
  - path: /usr/local/bin/set-virgl-resolution
    owner: root:root
    permissions: '0755'
    content: |
      #!/usr/bin/env bash
      set -euo pipefail

      output="$(xrandr --query | awk '/ connected/{print $1; exit}')"
      if [ -z "$output" ]; then
          exit 1
      fi
      xrandr --newmode "2560x1600_60.00" 348.50 \
          2560 2760 3032 3504 1600 1603 1609 1658 -hsync +vsync \
          2>/dev/null || true
      xrandr --addmode "$output" "2560x1600_60.00" 2>/dev/null || true
      xrandr --output "$output" --mode "2560x1600_60.00"
  - path: /etc/xdg/autostart/set-virgl-resolution.desktop
    owner: root:root
    permissions: '0644'
    content: |
      [Desktop Entry]
      Type=Application
      Name=Set VirGL display resolution
      Exec=/usr/local/bin/set-virgl-resolution
      OnlyShowIn=XFCE;
      X-GNOME-Autostart-enabled=true
  - path: /etc/systemd/system/home-gamer-shared.mount
    owner: root:root
    permissions: '0644'
    content: |
      [Unit]
      Description=Mount the Tirith repository
      Before=lightdm.service

      [Mount]
      What=repository
      Where=/home/gamer/shared
      Type=9p
      Options=trans=virtio,version=9p2000.L,msize=104857600,ro
      TimeoutSec=30

      [Install]
      WantedBy=multi-user.target
  - path: /etc/systemd/system/mnt-steamroot.mount
    owner: root:root
    permissions: '0644'
    content: |
      [Unit]
      Description=Mount host Steam metadata

      [Mount]
      What=steamroot
      Where=/mnt/steamroot
      Type=9p
      Options=trans=virtio,version=9p2000.L,msize=104857600,ro
      TimeoutSec=30

      [Install]
      WantedBy=multi-user.target
  - path: /etc/systemd/system/mnt-steamdata.mount
    owner: root:root
    permissions: '0644'
    content: |
      [Unit]
      Description=Mount host Steam installation

      [Mount]
      What=steamdata
      Where=/mnt/steamdata
      Type=9p
      Options=trans=virtio,version=9p2000.L,msize=104857600,ro
      TimeoutSec=30

      [Install]
      WantedBy=multi-user.target

runcmd:
  - [ passwd, -d, gamer ]
  - [ install, -d, -m, '0755', -o, gamer, -g, gamer, /home/gamer/shared ]
  - [ install, -d, -m, '0755', /mnt/steamroot ]
  - [ install, -d, -m, '0755', /mnt/steamdata ]
  - [ systemctl, daemon-reload ]
  - [ systemctl, enable, --now, qemu-guest-agent.service ]
  - [ systemctl, enable, --now, ssh.service ]
  - [ systemctl, enable, --now, home-gamer-shared.mount ]
  - [ systemctl, enable, --now, mnt-steamroot.mount ]
  - [ systemctl, enable, --now, mnt-steamdata.mount ]
  - [ ln, -sfn, /home/gamer/shared/virgl/sync-tf2-guest.sh, /usr/local/bin/sync-tf2 ]
  - [ ln, -sfn, /home/gamer/shared/virgl/run-tf2-guest.sh, /usr/local/bin/run-tf2 ]
  - [ systemctl, set-default, graphical.target ]
  - [ systemctl, enable, lightdm.service ]
  - [ systemctl, start, --no-block, lightdm.service ]

final_message: "VirGL TF2 VM is ready. SSH as gamer with an empty password, then run sync-tf2."
EOF

cloud-localds "$SEED_IMAGE" \
    "$CLOUD_INIT_DIR/user-data" "$CLOUD_INIT_DIR/meta-data"

echo
echo "VM assets are ready."
echo "Start the VM with: $SCRIPT_DIR/run-vm.sh"
