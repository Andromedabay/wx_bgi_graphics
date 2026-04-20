#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_hint="$(cd -- "${script_dir}/../../.." && pwd)"

show_usage() {
    cat <<'EOF'
Usage: run_openlb_pipe_3d_demo.sh [options]

Build and run the wx_bgi_graphics 3D OpenLB pipe demo on Debian, Ubuntu,
or Debian/Ubuntu-based WSL2.

Options:
  --test                  Build and run the demo's short validation mode.
  --vtk[=N]               Enable VTK export for the first N iterations (default: 100).
  --vtk-iterations N      Set the VTK export iteration count and enable VTK output.
  --sieve-hole-mm N       Set the requested sieve hole diameter in millimetres.
  --flow-velocity-ms N    Set the characteristic flow velocity in m/s.
  --skip-system-packages  Skip apt-based dependency installation.
  --force-clone           Ignore the current checkout and clone wx_bgi_graphics into /tmp.
  --help                  Show this help text.

Environment overrides:
  WXBGI_ROOT   Existing wx_bgi_graphics checkout to build.
  OPENLB_ROOT  OpenLB checkout path (default: /tmp/openlb-release).
  BUILD_DIR    CMake build directory (default: /tmp/wx_bgi_graphics-openlb-build).
EOF
}

log() {
    printf '[openlb-demo] %s\n' "$*"
}

fail() {
    printf '[openlb-demo] ERROR: %s\n' "$*" >&2
    exit 1
}

test_mode=0
skip_system_packages=0
force_clone=0
declare -a demo_args=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --test)
            test_mode=1
            ;;
        --vtk|--vtk=*)
            demo_args+=("$1")
            ;;
        --vtk-iterations)
            [[ $# -ge 2 ]] || fail "--vtk-iterations requires a positive integer argument"
            demo_args+=("$1" "$2")
            shift
            ;;
        --vtk-iterations=*)
            demo_args+=("$1")
            ;;
        --sieve-hole-mm)
            [[ $# -ge 2 ]] || fail "--sieve-hole-mm requires a numeric argument"
            demo_args+=("$1" "$2")
            shift
            ;;
        --sieve-hole-mm=*)
            demo_args+=("$1")
            ;;
        --flow-velocity-ms|--flow-ms)
            [[ $# -ge 2 ]] || fail "--flow-velocity-ms requires a numeric argument"
            demo_args+=("$1" "$2")
            shift
            ;;
        --flow-velocity-ms=*|--flow-ms=*)
            demo_args+=("$1")
            ;;
        --skip-system-packages)
            skip_system_packages=1
            ;;
        --force-clone)
            force_clone=1
            ;;
        --help|-h)
            show_usage
            exit 0
            ;;
        *)
            fail "unknown option: $1"
            ;;
    esac
    shift
done

ensure_debian_like() {
    [[ -r /etc/os-release ]] || fail "cannot detect Linux distribution (/etc/os-release missing)"
    # shellcheck disable=SC1091
    source /etc/os-release
    if [[ "${ID:-}" != "debian" && "${ID:-}" != "ubuntu" && "${ID_LIKE:-}" != *"debian"* ]]; then
        fail "this script currently supports Debian/Ubuntu-family systems only"
    fi
}

run_as_root() {
    if [[ "$(id -u)" -eq 0 ]]; then
        "$@"
    elif command -v sudo >/dev/null 2>&1; then
        sudo "$@"
    else
        fail "need root privileges or sudo to install packages"
    fi
}

install_packages() {
    if [[ "${skip_system_packages}" -eq 1 ]]; then
        log "skipping apt package installation"
        return
    fi

    ensure_debian_like
    export DEBIAN_FRONTEND=noninteractive
    local packages=(
        build-essential
        ca-certificates
        cmake
        git
        libgl1-mesa-dev
        libglu1-mesa-dev
        libglfw3-dev
        libgtk-3-dev
        libwxgtk3.2-dev
        libx11-dev
        libxcursor-dev
        libxi-dev
        libxinerama-dev
        libxrandr-dev
        libxxf86vm-dev
        pkg-config
        zlib1g-dev
    )

    log "installing Debian/Ubuntu build dependencies"
    run_as_root apt-get update
    run_as_root apt-get install -y "${packages[@]}"
}

resolve_wxbgi_root() {
    if [[ -n "${WXBGI_ROOT:-}" ]]; then
        printf '%s\n' "${WXBGI_ROOT}"
        return
    fi

    if [[ "${force_clone}" -eq 0 && -f "${repo_hint}/CMakeLists.txt" ]]; then
        printf '%s\n' "${repo_hint}"
        return
    fi

    printf '%s\n' "/tmp/wx_bgi_graphics-openlb-demo"
}

clone_wxbgi_if_needed() {
    local root="$1"
    if [[ -f "${root}/CMakeLists.txt" ]]; then
        return
    fi

    log "cloning wx_bgi_graphics into ${root}"
    rm -rf "${root}"
    git clone --depth 1 https://github.com/Andromedabay/wx_bgi_graphics.git "${root}"
}

clone_openlb_if_needed() {
    local root="$1"
    if [[ -f "${root}/src/olb.h" ]]; then
        log "reusing OpenLB checkout at ${root}"
        return
    fi

    log "cloning latest public OpenLB release into ${root}"
    rm -rf "${root}"
    git clone https://gitlab.com/openlb/release.git "${root}"
}

cmake_args_for_system_libs() {
    local args=()
    if command -v wx-config >/dev/null 2>&1; then
        args+=(-DWXBGI_SYSTEM_WX=ON)
    fi
    if pkg-config --exists glfw3 2>/dev/null; then
        args+=(-DWXBGI_SYSTEM_GLFW=ON)
    fi
    printf '%s\n' "${args[@]}"
}

ensure_gui_available() {
    if [[ "${test_mode}" -eq 1 ]]; then
        return
    fi
    if [[ -n "${DISPLAY:-}" || -n "${WAYLAND_DISPLAY:-}" ]]; then
        return
    fi
    fail "no GUI display detected; use --test for a short validation run or start a GUI session/WSLg"
}

main() {
    install_packages

    local wxbgi_root
    wxbgi_root="$(resolve_wxbgi_root)"
    clone_wxbgi_if_needed "${wxbgi_root}"

    local openlb_root="${OPENLB_ROOT:-/tmp/openlb-release}"
    local build_dir="${BUILD_DIR:-/tmp/wx_bgi_graphics-openlb-build}"
    clone_openlb_if_needed "${openlb_root}"

    ensure_gui_available

    local -a cmake_args=(
        -S "${wxbgi_root}"
        -B "${build_dir}"
        -DCMAKE_BUILD_TYPE=Release
        -DWXBGI_ENABLE_OPENLB=ON
        -DOPENLB_ROOT="${openlb_root}"
    )

    while IFS= read -r arg; do
        [[ -n "${arg}" ]] && cmake_args+=("${arg}")
    done < <(cmake_args_for_system_libs)

    log "configuring CMake build in ${build_dir}"
    cmake "${cmake_args[@]}"

    log "building wxbgi_openlb_pipe_3d_demo"
    cmake --build "${build_dir}" -j --target wxbgi_openlb_pipe_3d_demo

    local demo_bin="${build_dir}/wxbgi_openlb_pipe_3d_demo"
    [[ -x "${demo_bin}" ]] || fail "demo binary not found at ${demo_bin}"

    if [[ "${test_mode}" -eq 1 ]]; then
        log "running test mode"
        exec "${demo_bin}" --test "${demo_args[@]}"
    fi

    log "running interactive demo"
    exec "${demo_bin}" --smooth "${demo_args[@]}"
}

main "$@"
