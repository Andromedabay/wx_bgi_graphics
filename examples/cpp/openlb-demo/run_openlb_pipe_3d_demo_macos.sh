#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_hint="$(cd -- "${script_dir}/../../.." && pwd)"

show_usage() {
    cat <<'EOF'
Usage: run_openlb_pipe_3d_demo_macos.sh [options]

Build and run the wx_bgi_graphics 3D OpenLB pipe demo on macOS.
Assumes Homebrew is already installed.

Options:
  --test                  Build and run the demo's short validation mode.
  --skip-system-packages  Skip brew-based dependency installation.
  --force-clone           Ignore the current checkout and clone wx_bgi_graphics into /tmp.
  --help                  Show this help text.

Environment overrides:
  WXBGI_ROOT   Existing wx_bgi_graphics checkout to build.
  OPENLB_ROOT  OpenLB checkout path (default: /tmp/openlb-release).
  BUILD_DIR    CMake build directory (default: /tmp/wx_bgi_graphics-openlb-build-macos).
EOF
}

log() {
    printf '[openlb-demo-macos] %s\n' "$*"
}

fail() {
    printf '[openlb-demo-macos] ERROR: %s\n' "$*" >&2
    exit 1
}

test_mode=0
skip_system_packages=0
force_clone=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --test)
            test_mode=1
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

ensure_macos() {
    [[ "$(uname -s)" == "Darwin" ]] || fail "this script only supports macOS"
}

ensure_brew() {
    command -v brew >/dev/null 2>&1 || fail "Homebrew is required but was not found in PATH"
}

install_packages() {
    if [[ "${skip_system_packages}" -eq 1 ]]; then
        log "skipping brew package installation"
        return
    fi

    ensure_brew
    local packages=(
        cmake
        git
        glfw
        pkg-config
        wxwidgets
    )
    log "installing Homebrew build dependencies"
    brew install "${packages[@]}"
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

configure_brew_env() {
    ensure_brew

    local brew_prefix
    local wx_prefix
    local glfw_prefix

    brew_prefix="$(brew --prefix)"
    wx_prefix="$(brew --prefix wxwidgets)"
    glfw_prefix="$(brew --prefix glfw)"

    export PATH="${brew_prefix}/bin:${PATH}"
    export PKG_CONFIG_PATH="${glfw_prefix}/lib/pkgconfig:${wx_prefix}/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
    export CMAKE_PREFIX_PATH="${glfw_prefix}:${wx_prefix}:${CMAKE_PREFIX_PATH:-}"

    if [[ -x "${wx_prefix}/bin/wx-config" ]]; then
        export WX_CONFIG="${wx_prefix}/bin/wx-config"
        export PATH="${wx_prefix}/bin:${PATH}"
    fi
}

ensure_gui_available() {
    if [[ "${test_mode}" -eq 1 ]]; then
        return
    fi
    if [[ -n "${SSH_CONNECTION:-}" || -n "${SSH_TTY:-}" ]]; then
        fail "interactive mode should be run from a local macOS desktop session; use --test over SSH"
    fi
}

main() {
    ensure_macos
    install_packages
    configure_brew_env

    local wxbgi_root
    wxbgi_root="$(resolve_wxbgi_root)"
    clone_wxbgi_if_needed "${wxbgi_root}"

    local openlb_root="${OPENLB_ROOT:-/tmp/openlb-release}"
    local build_dir="${BUILD_DIR:-/tmp/wx_bgi_graphics-openlb-build-macos}"
    clone_openlb_if_needed "${openlb_root}"

    ensure_gui_available

    local -a cmake_args=(
        -S "${wxbgi_root}"
        -B "${build_dir}"
        -DCMAKE_BUILD_TYPE=Debug
        -DWXBGI_ENABLE_OPENLB=ON
        -DOPENLB_ROOT="${openlb_root}"
        -DWXBGI_SYSTEM_WX=ON
        -DWXBGI_SYSTEM_GLFW=ON
    )

    log "configuring CMake build in ${build_dir}"
    cmake "${cmake_args[@]}"

    log "building wxbgi_openlb_pipe_3d_demo"
    cmake --build "${build_dir}" -j --target wxbgi_openlb_pipe_3d_demo

    local demo_bin="${build_dir}/wxbgi_openlb_pipe_3d_demo"
    [[ -x "${demo_bin}" ]] || fail "demo binary not found at ${demo_bin}"

    if [[ "${test_mode}" -eq 1 ]]; then
        log "running test mode"
        exec "${demo_bin}" --test
    fi

    log "running interactive demo"
    exec "${demo_bin}" --smooth
}

main "$@"
