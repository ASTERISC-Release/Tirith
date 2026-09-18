#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$ROOT_DIR/src"
BUILD_DIR="$ROOT_DIR/build"
BIN_DIR="$ROOT_DIR/bin"
DEPS_DIR="$ROOT_DIR/deps"

MUVM_RELEASE_API="${MUVM_RELEASE_API:-https://api.github.com/repos/AsahiLinux/muvm/releases/latest}"
MUVM_RELEASE_TAG="${MUVM_RELEASE_TAG:-}"
VIRGL_REPO="${VIRGL_REPO:-https://gitlab.freedesktop.org/virgl/virglrenderer.git}"
LIBKRUN_RELEASE_API="${LIBKRUN_RELEASE_API:-https://api.github.com/repos/containers/libkrun/releases/latest}"
LIBKRUN_RELEASE_TAG="${LIBKRUN_RELEASE_TAG:-}"
LIBKRUNFW_RELEASE_API="${LIBKRUNFW_RELEASE_API:-https://api.github.com/repos/containers/libkrunfw/releases/latest}"
LIBKRUNFW_RELEASE_TAG="${LIBKRUNFW_RELEASE_TAG:-}"
SKIP_DEPS_INSTALL="${SKIP_DEPS_INSTALL:-0}"

die() {
    echo "error: $*" >&2
    exit 1
}

usage() {
    cat >&2 <<EOF
usage: $0 [clean]

  no args  install dependencies, fetch/build muvm and virglrenderer
  clean    remove generated portable build artifacts
EOF
    exit 2
}

need_cmd() {
    command -v "$1" >/dev/null 2>&1 || die "missing required command: $1"
}

version_ge() {
    local lhs=$1
    local rhs=$2

    [[ "$(printf '%s\n%s\n' "$rhs" "$lhs" | sort -V | head -n 1)" == "$rhs" ]]
}

detect_distro() {
    if [[ ! -r /etc/os-release ]]; then
        die "cannot detect distro: /etc/os-release is missing"
    fi

    # shellcheck disable=SC1091
    source /etc/os-release
    DISTRO_ID="${ID:-unknown}"
    DISTRO_LIKE="${ID_LIKE:-}"
}

install_deps_arch() {
    if [[ "$SKIP_DEPS_INSTALL" == "1" ]]; then
        return 0
    fi

    sudo pacman -Sy --needed --noconfirm \
        base-devel \
        git \
        rust \
        clang \
        llvm \
        pkgconf \
        python \
        python-pyyaml \
        meson \
        ninja \
        libdrm \
        libepoxy \
        mesa \
        libx11 \
        libxext \
        libxfixes \
        libxdamage \
        libxshmfence \
        libxxf86vm \
        vulkan-headers \
        libkrun \
        passt \
        socat \
        mesa-utils \
        vulkan-virtio \
        vulkan-radeon
}

install_deps_ubuntu() {
    if [[ "$SKIP_DEPS_INSTALL" == "1" ]]; then
        return 0
    fi

    sudo apt-get update
    sudo apt-get install -y \
        build-essential \
        git \
        curl \
        pkg-config \
        clang \
        llvm-dev \
        libclang-dev \
        python3 \
        python3-yaml \
        meson \
        ninja-build \
        libdrm-dev \
        libepoxy-dev \
        libgbm-dev \
        libx11-dev \
        libxext-dev \
        libxfixes-dev \
        libxdamage-dev \
        libxshmfence-dev \
        libxxf86vm-dev \
        libwayland-dev \
        libvulkan-dev \
        mesa-utils \
        mesa-vulkan-drivers \
        passt \
        socat \
        patchelf \
        libcap-ng-dev \
        libglib2.0-dev
}

get_required_muvm_rust_version() {
    local manifest=$1

    python3 - "$manifest" <<'PY'
import re
import sys

with open(sys.argv[1], "r", encoding="utf-8") as f:
    for line in f:
        match = re.match(r'\s*rust-version\s*=\s*"([^"]+)"', line)
        if match:
            print(match.group(1))
            break
    else:
        raise SystemExit("could not find rust-version in Cargo.toml")
PY
}

install_rustup_toolchain() {
    need_cmd curl

    if [[ ! -x "$HOME/.cargo/bin/rustc" ]]; then
        RUSTUP_INIT_SKIP_PATH_CHECK=yes \
            curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs \
            | sh -s -- -y --profile minimal --default-toolchain stable
    fi

    export PATH="$HOME/.cargo/bin:$PATH"
    "$HOME/.cargo/bin/rustup" toolchain install stable --profile minimal >/dev/null
}

ensure_rust() {
    local manifest=$1
    local required_version current_version=""

    required_version="$(get_required_muvm_rust_version "$manifest")"
    export PATH="$HOME/.cargo/bin:$PATH"

    if command -v rustc >/dev/null 2>&1; then
        current_version="$(rustc --version | awk '{print $2}')"
    fi

    if [[ -z "$current_version" ]] || ! version_ge "$current_version" "$required_version"; then
        install_rustup_toolchain
        current_version="$(rustc --version | awk '{print $2}')"
    fi

    need_cmd cargo
    need_cmd rustc
    version_ge "$current_version" "$required_version" \
        || die "rustc $required_version or newer is required, found $current_version"
}

clone_or_update() {
    local repo_url=$1
    local dest=$2

    if [[ -d "$dest/.git" ]]; then
        git -C "$dest" fetch --tags --prune
        git -C "$dest" pull --ff-only
    else
        git clone "$repo_url" "$dest"
    fi
}

get_muvm_release_tag() {
    if [[ -n "$MUVM_RELEASE_TAG" ]]; then
        printf '%s\n' "$MUVM_RELEASE_TAG"
        return 0
    fi

    curl -fsSL "$MUVM_RELEASE_API" \
        | python3 -c 'import json, sys; print(json.load(sys.stdin)["tag_name"])'
}

get_libkrun_release_tag() {
    if [[ -n "$LIBKRUN_RELEASE_TAG" ]]; then
        printf '%s\n' "$LIBKRUN_RELEASE_TAG"
        return 0
    fi

    curl -fsSL "$LIBKRUN_RELEASE_API" \
        | python3 -c 'import json, sys; print(json.load(sys.stdin)["tag_name"])'
}

get_libkrunfw_release_tag() {
    if [[ -n "$LIBKRUNFW_RELEASE_TAG" ]]; then
        printf '%s\n' "$LIBKRUNFW_RELEASE_TAG"
        return 0
    fi

    curl -fsSL "$LIBKRUNFW_RELEASE_API" \
        | python3 -c 'import json, sys; print(json.load(sys.stdin)["tag_name"])'
}

fetch_muvm_release() {
    local dest="$SRC_DIR/muvm"
    local tag archive_url archive tmpdir stamp_file extracted

    tag="$(get_muvm_release_tag)"
    archive_url="https://github.com/AsahiLinux/muvm/archive/refs/tags/${tag}.tar.gz"
    stamp_file="$dest/.portable-release-tag"

    if [[ -f "$stamp_file" ]] && [[ "$(cat "$stamp_file")" == "$tag" ]]; then
        MUVM_RELEASE_TAG_RESOLVED="$tag"
        return 0
    fi

    tmpdir="$(mktemp -d)"
    archive="$tmpdir/muvm-${tag}.tar.gz"

    rm -rf "$dest"
    mkdir -p "$SRC_DIR"

    curl -fsSL "$archive_url" -o "$archive"
    tar -C "$tmpdir" -xf "$archive"

    extracted="$(find "$tmpdir" -maxdepth 1 -mindepth 1 -type d -name 'muvm-*' | head -n 1)"
    [[ -n "$extracted" ]] || die "failed to unpack muvm release archive for $tag"

    mv "$extracted" "$dest"
    printf '%s\n' "$tag" >"$stamp_file"
    rm -rf "$tmpdir"

    MUVM_RELEASE_TAG_RESOLVED="$tag"
}

patch_muvm_root_guard_for_ubuntu() {
    local file="$SRC_DIR/muvm/crates/muvm/src/bin/muvm.rs"

    [[ -f "$file" ]] || die "missing muvm source file to patch: $file"

    python3 - "$file" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text(encoding="utf-8")

old = """    if getuid().as_raw() == 0 || geteuid().as_raw() == 0 {
        println!(\"Running as root is not supported as it may break your system\");
        return Err(anyhow!(\"real user ID or effective user ID is 0\"));
    }
"""

new = """    let allow_root = env::var_os(\"MUVM_ALLOW_ROOT\").as_deref() == Some(\"1\".as_ref());
    if (getuid().as_raw() == 0 || geteuid().as_raw() == 0) && !allow_root {
        println!(\"Running as root is not supported as it may break your system\");
        return Err(anyhow!(\"real user ID or effective user ID is 0\"));
    }
"""

if new in text:
    raise SystemExit(0)

if old not in text:
    raise SystemExit(f"expected root guard not found in {path}")

path.write_text(text.replace(old, new, 1), encoding="utf-8")
PY
}

local_lib_paths() {
    LOCAL_DEPS_LIB_DIR="$DEPS_DIR/lib"
    LOCAL_DEPS_PKGCONFIG_DIR="$LOCAL_DEPS_LIB_DIR/pkgconfig"
}

activate_local_libs() {
    local_lib_paths

    if [[ -d "$LOCAL_DEPS_PKGCONFIG_DIR" ]]; then
        export PKG_CONFIG_PATH="$LOCAL_DEPS_PKGCONFIG_DIR${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
    fi

    if [[ -d "$LOCAL_DEPS_LIB_DIR" ]]; then
        export LD_LIBRARY_PATH="$LOCAL_DEPS_LIB_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    fi
}

normalize_libkrunfw_arch() {
    case "$(uname -m)" in
        x86_64|amd64)
            printf '%s\n' x86_64
            ;;
        aarch64|arm64)
            printf '%s\n' aarch64
            ;;
        riscv64)
            printf '%s\n' riscv64
            ;;
        *)
            die "unsupported architecture for libkrunfw: $(uname -m)"
            ;;
    esac
}

fetch_libkrun_release() {
    local dest="$SRC_DIR/libkrun"
    local tag archive_url archive tmpdir stamp_file extracted

    tag="$(get_libkrun_release_tag)"
    archive_url="https://github.com/containers/libkrun/archive/refs/tags/${tag}.tar.gz"
    stamp_file="$dest/.portable-release-tag"

    if [[ -f "$stamp_file" ]] && [[ "$(cat "$stamp_file")" == "$tag" ]]; then
        LIBKRUN_RELEASE_TAG_RESOLVED="$tag"
        return 0
    fi

    tmpdir="$(mktemp -d)"
    archive="$tmpdir/libkrun-${tag}.tar.gz"

    rm -rf "$dest"
    mkdir -p "$SRC_DIR"

    curl -fsSL "$archive_url" -o "$archive"
    tar -C "$tmpdir" -xf "$archive"

    extracted="$(find "$tmpdir" -maxdepth 1 -mindepth 1 -type d -name 'libkrun-*' | head -n 1)"
    [[ -n "$extracted" ]] || die "failed to unpack libkrun release archive for $tag"

    mv "$extracted" "$dest"
    printf '%s\n' "$tag" >"$stamp_file"
    rm -rf "$tmpdir"

    LIBKRUN_RELEASE_TAG_RESOLVED="$tag"
}

install_local_libkrunfw() {
    local tag arch archive_url archive tmpdir stamp_file

    local_lib_paths
    tag="$(get_libkrunfw_release_tag)"
    arch="$(normalize_libkrunfw_arch)"
    archive_url="https://github.com/containers/libkrunfw/releases/download/${tag}/libkrunfw-${arch}.tgz"
    stamp_file="$DEPS_DIR/.libkrunfw-release-tag"

    if [[ -f "$stamp_file" ]] \
        && [[ "$(cat "$stamp_file")" == "$tag" ]] \
        && [[ -f "$LOCAL_DEPS_LIB_DIR/libkrunfw.so" ]]; then
        LIBKRUNFW_RELEASE_TAG_RESOLVED="$tag"
        return 0
    fi

    tmpdir="$(mktemp -d)"
    archive="$tmpdir/libkrunfw-${tag}-${arch}.tgz"

    mkdir -p "$LOCAL_DEPS_LIB_DIR"
    curl -fsSL "$archive_url" -o "$archive"
    tar -C "$tmpdir" -xf "$archive"

    rm -f "$LOCAL_DEPS_LIB_DIR"/libkrunfw.so*
    cp -a "$tmpdir/lib64/." "$LOCAL_DEPS_LIB_DIR/"
    printf '%s\n' "$tag" >"$stamp_file"
    rm -rf "$tmpdir"

    LIBKRUNFW_RELEASE_TAG_RESOLVED="$tag"
}

build_local_libkrun() {
    local prefix="$DEPS_DIR"
    local stamp_file="$DEPS_DIR/.libkrun-release-tag"

    fetch_libkrun_release
    install_local_libkrunfw
    activate_local_libs

    if [[ -f "$stamp_file" ]] \
        && [[ "$(cat "$stamp_file")" == "$LIBKRUN_RELEASE_TAG_RESOLVED" ]] \
        && [[ -f "$prefix/lib/libkrun.so" ]]; then
        return 0
    fi

    mkdir -p "$prefix"
    (
        cd "$SRC_DIR/libkrun"
        make clean >/dev/null 2>&1 || true
        make GPU=1 NET=1 BLK=1 PREFIX="$prefix" LIBDIR_Linux=lib -j"$(nproc)"
        make GPU=1 NET=1 BLK=1 PREFIX="$prefix" LIBDIR_Linux=lib install
    )

    printf '%s\n' "$LIBKRUN_RELEASE_TAG_RESOLVED" >"$stamp_file"
}

maybe_prepare_libkrun() {
    activate_local_libs

    if pkg-config --exists libkrun; then
        return 0
    fi

    if [[ "$DISTRO_ID" == "arch" ]]; then
        die "libkrun should have been installed from pacman, but pkg-config still cannot find it"
    fi

    build_local_libkrun
    activate_local_libs
    pkg-config --exists libkrun || die "failed to prepare local libkrun for Ubuntu"
}

build_virglrenderer() {
    local src="$SRC_DIR/virglrenderer"
    local build="$src/build-amdgpu"

    meson setup "$build" "$src" \
        --reconfigure \
        -Dvenus=true \
        -Ddrm-renderers=amdgpu-experimental
    meson compile -C "$build"
}

build_muvm() {
    local src="$SRC_DIR/muvm"

    activate_local_libs

    (
        cd "$src"
        cargo build --release
    )

    mkdir -p "$BIN_DIR"
    install -m 0755 "$src/target/release/muvm" "$BIN_DIR/muvm"
    install -m 0755 "$src/target/release/muvm-guest" "$BIN_DIR/muvm-guest"
}

write_env_file() {
    local env_file="$ROOT_DIR/env.sh"
    local virgl_build="$SRC_DIR/virglrenderer/build-amdgpu"
    local deps_lib="$DEPS_DIR/lib"
    local deps_pkgconfig="$deps_lib/pkgconfig"

    cat >"$env_file" <<EOF
#!/usr/bin/env bash
export PATH="$BIN_DIR:\$PATH"
export MUVM_PORTABLE_ROOT="$ROOT_DIR"
export MUVM_VIRGL_BUILD="$virgl_build"
if [[ -d "$deps_lib" ]]; then
    export LD_LIBRARY_PATH="$deps_lib\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
fi
if [[ -d "$deps_pkgconfig" ]]; then
    export PKG_CONFIG_PATH="$deps_pkgconfig\${PKG_CONFIG_PATH:+:\$PKG_CONFIG_PATH}"
fi
EOF
    chmod +x "$env_file"
}

clean_portable() {
    rm -rf "$SRC_DIR" "$BUILD_DIR" "$BIN_DIR" "$DEPS_DIR" "$ROOT_DIR/env.sh"
    echo "portable state removed from $ROOT_DIR"
}

main() {
    if [[ $# -gt 1 ]]; then
        usage
    fi

    if [[ $# -eq 1 ]]; then
        case "$1" in
            clean)
                clean_portable
                return 0
                ;;
            -h|--help|help)
                usage
                ;;
            *)
                usage
                ;;
        esac
    fi

    detect_distro

    mkdir -p "$SRC_DIR" "$BUILD_DIR" "$BIN_DIR" "$DEPS_DIR"

    case "$DISTRO_ID" in
        arch)
            install_deps_arch
            ;;
        ubuntu)
            install_deps_ubuntu
            ;;
        *)
            if [[ " $DISTRO_LIKE " == *" ubuntu "* ]] || [[ " $DISTRO_LIKE " == *" debian "* ]]; then
                install_deps_ubuntu
            else
                die "unsupported distro: $DISTRO_ID"
            fi
            ;;
    esac

    fetch_muvm_release
    if [[ "$DISTRO_ID" == "ubuntu" ]] || [[ " $DISTRO_LIKE " == *" ubuntu "* ]] || [[ " $DISTRO_LIKE " == *" debian "* ]]; then
        patch_muvm_root_guard_for_ubuntu
    fi
    ensure_rust "$SRC_DIR/muvm/crates/muvm/Cargo.toml"
    clone_or_update "$VIRGL_REPO" "$SRC_DIR/virglrenderer"
    maybe_prepare_libkrun
    build_virglrenderer
    build_muvm
    write_env_file

    cat <<EOF
portable setup complete

root:        $ROOT_DIR
muvm source: $SRC_DIR/muvm
muvm tag:    $MUVM_RELEASE_TAG_RESOLVED
virgl source:$SRC_DIR/virglrenderer
bin dir:     $BIN_DIR
deps dir:    $DEPS_DIR

Next:
  source "$ROOT_DIR/env.sh"
  "$ROOT_DIR/run.sh" glxinfo venus -B
  "$ROOT_DIR/run.sh" glxinfo drm-native -B
EOF
}

main "$@"
