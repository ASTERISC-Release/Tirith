#!/bin/bash -e

# install podman (if needed)
sudo pacman -S --needed podman podman-compose

if [ "$1" == "down" ]; then
    echo "Container being stopped"
    podman-compose down
fi

# Check if container is already running
if podman ps --format "{{.Names}}" | grep -q "^ubuntu-gpu-kvm$"; then
    echo "Container already running, attaching..."
    elif podman ps -a --format "{{.Names}}" | grep -q "^ubuntu-gpu-kvm$"; then
    # Container exists but is stopped, start it
    echo "Starting existing container..."
    podman start ubuntu-gpu-kvm
else
    # Container doesn't exist, build and start it
    echo "Building and starting new container..."
    podman-compose build
    podman-compose up -d
fi

# Attach to the container
podman exec -it ubuntu-gpu-kvm bash