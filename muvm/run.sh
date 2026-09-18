#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="$ROOT_DIR/bin"
VIRGL_BUILD="$ROOT_DIR/src/virglrenderer/build-amdgpu"
DEPS_LIB_DIR="$ROOT_DIR/deps/lib"
export vblank_mode=0

usage() {
    cat >&2 <<EOF
usage: $0 APPLICATION BACKEND [APP_ARGS...]

BACKEND:
  venus
  drm-native

examples:
  $0 glxgears venus
  $0 glxinfo drm-native -B
EOF
    exit 2
}

need_file() {
    [[ -e "$1" ]] || {
        echo "error: missing $1" >&2
        echo "run $ROOT_DIR/setup.sh first" >&2
        exit 1
    }
}

is_ubuntu_like() {
    [[ -r /etc/os-release ]] || return 1
    local distro_id="" distro_like=""

    # shellcheck disable=SC1091
    source /etc/os-release
    distro_id="${ID:-}"
    distro_like=" ${ID_LIKE:-} "

    [[ "$distro_id" == "ubuntu" ]] \
        || [[ "$distro_like" == *" ubuntu "* ]] \
        || [[ "$distro_like" == *" debian "* ]]
}

find_virtio_icd() {
    local candidates=(
        /usr/share/vulkan/icd.d/virtio_icd.json
        /usr/share/vulkan/icd.d/virtio_icd.x86_64.json
        /etc/vulkan/icd.d/virtio_icd.json
        /etc/vulkan/icd.d/virtio_icd.x86_64.json
    )
    local candidate

    for candidate in "${candidates[@]}"; do
        if [[ -f "$candidate" ]]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done

    return 1
}

main() {
    [[ $# -ge 2 ]] || usage

    local app=$1
    local backend=$2
    local runtime_dir=""
    local -a muvm_args=(-i)
    local -a guest_env_args=(
        -e "vblank_mode=${vblank_mode:-0}"
    )
    shift 2

    need_file "$BIN_DIR/muvm"
    need_file "$BIN_DIR/muvm-guest"

    export PATH="$BIN_DIR:$PATH"
    if [[ $(id -u) -eq 0 ]]; then
        export MUVM_ALLOW_ROOT=1
        runtime_dir="${MUVM_ROOT_XDG_RUNTIME_DIR:-${TMPDIR:-/tmp}/muvm-runtime-root-$$}"
        export XDG_RUNTIME_DIR="$runtime_dir"
        rm -rf "$XDG_RUNTIME_DIR"
        mkdir -p "$XDG_RUNTIME_DIR"
        chmod 700 "$XDG_RUNTIME_DIR"
    fi
    if [[ -d "$DEPS_LIB_DIR" ]]; then
        export LD_LIBRARY_PATH="$DEPS_LIB_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    fi
    if [[ -t 0 && -t 1 ]]; then
        muvm_args+=(-t)
    fi

    case "$backend" in
        venus)
            local virtio_icd
            virtio_icd="$(find_virtio_icd)" || {
                echo "error: could not find a virtio Vulkan ICD (virtio_icd.json)" >&2
                echo "install the distro's Venus userspace package first" >&2
                exit 1
            }

            need_file "$VIRGL_BUILD/src/libvirglrenderer.so"
            need_file "$VIRGL_BUILD/server/virgl_render_server"
            export LD_LIBRARY_PATH="$VIRGL_BUILD/src${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
            export RENDER_SERVER_EXEC_PATH="$VIRGL_BUILD/server/virgl_render_server"

            exec "$BIN_DIR/muvm" \
                "${muvm_args[@]}" \
                --gpu-mode=venus \
                "${guest_env_args[@]}" \
                -e "VK_DRIVER_FILES=$virtio_icd" \
                "$app" "$@"
            ;;
        drm-native)
            if is_ubuntu_like && [[ "${MUVM_UBUNTU_DRM_MODE:-auto}" != "native" ]]; then
                echo "note: stock Ubuntu Mesa's DRM native context path is unstable here; using Venus for drm-native" >&2
                echo "note: set MUVM_UBUNTU_DRM_MODE=native to force the real drm backend" >&2
                backend=venus
                local virtio_icd
                virtio_icd="$(find_virtio_icd)" || {
                    echo "error: could not find a virtio Vulkan ICD (virtio_icd.json)" >&2
                    echo "install the distro's Venus userspace package first" >&2
                    exit 1
                }

                need_file "$VIRGL_BUILD/src/libvirglrenderer.so"
                need_file "$VIRGL_BUILD/server/virgl_render_server"
                export LD_LIBRARY_PATH="$VIRGL_BUILD/src${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
                export RENDER_SERVER_EXEC_PATH="$VIRGL_BUILD/server/virgl_render_server"

                exec "$BIN_DIR/muvm" \
                    "${muvm_args[@]}" \
                    --gpu-mode=venus \
                    "${guest_env_args[@]}" \
                    -e "VK_DRIVER_FILES=$virtio_icd" \
                    "$app" "$@"
            fi

            need_file "$VIRGL_BUILD/src/libvirglrenderer.so"
            need_file "$VIRGL_BUILD/server/virgl_render_server"

            export LD_LIBRARY_PATH="$VIRGL_BUILD/src${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
            export RENDER_SERVER_EXEC_PATH="$VIRGL_BUILD/server/virgl_render_server"

            exec "$BIN_DIR/muvm" \
                "${muvm_args[@]}" \
                --gpu-mode=drm \
                "${guest_env_args[@]}" \
                "$app" "$@"
            ;;
        *)
            echo "error: unknown backend: $backend" >&2
            usage
            ;;
    esac
}

main "$@"
