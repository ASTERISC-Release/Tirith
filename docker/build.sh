#!/bin/bash -e

# install podman (if needed)


# stop and remove any existing container/image
podman stop ubuntu-gpu-kvm || true
podman rm ubuntu-gpu-kvm || true
podman rmi localhost/docker_ubuntu-gpu-kvm || true

# rebuild the image
podman-compose build
podman-compose up -d

# podman-compose 1.6 accepts `ipc: host` in its parsed configuration but does
# not pass it to `podman create`. Recreate the otherwise identical container
# with the missing option when that compatibility workaround is needed.
if [ "$(podman inspect ubuntu-gpu-kvm --format '{{.HostConfig.IpcMode}}')" != "host" ]; then
    mapfile -t create_cmd < <(
        podman inspect ubuntu-gpu-kvm \
            --format '{{range .Config.CreateCommand}}{{println .}}{{end}}'
    )
    podman stop ubuntu-gpu-kvm
    podman rm ubuntu-gpu-kvm
    create_cmd=("${create_cmd[@]:0:2}" --ipc=host "${create_cmd[@]:2}")
    "${create_cmd[@]}"
    podman start ubuntu-gpu-kvm
fi
