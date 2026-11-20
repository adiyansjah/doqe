#!/bin/sh
# Run from the repository root: sh docs/adrs/0001-core-cpp-toolchain/refs/tool-inventory.sh
set -eu

report_version() {
    if version=$("$1" --version 2>&1); then
        version=$(printf '%s\n' "$version" |
            awk 'NR == 1 { first = $0 } /version [0-9]/ { print; found = 1; exit } END { if (!found) print first }')
        printf '%s: %s\n' "$1" "$version"
    else
        printf '%s: version query failed (%s)\n' "$1" "$(printf '%s\n' "$version" | sed -n '1p')"
    fi
}

report_libcxx() {
    printf 'libc++ header and C++23 feature macros (%s):\n' "$1"
    shift
    if macros=$("$@" -std=c++23 -stdlib=libc++ -dM -E -x c++ -include version /dev/null 2>&1); then
        printf '%s\n' "$macros" |
            LC_ALL=C sort |
            awk '$2 ~ /^(_LIBCPP_VERSION|__cpp_explicit_this_parameter|__cpp_lib_(expected|byteswap|unreachable|to_underlying))$/'
    else
        printf '  Header probe failed: %s\n' "$(printf '%s\n' "$macros" | sed -n '1p')"
    fi
}

report_llvm() {
    printf '\nLLVM tools in %s:\n' "$1"
    for tool in clang clang++ clang-tidy clang-format llvm-config; do
        if [ -x "$1/$tool" ]; then
            report_version "$1/$tool"
        else
            printf '%s: absent\n' "$1/$tool"
        fi
    done
    compiler="$1/clang++"
    shift
    report_libcxx 'Clang driver search paths' "$compiler" "$@"
}

report_host() {
    os=$(uname -s)
    printf 'Date: %s\n' "$(date '+%Y-%m-%dT%H:%M:%S%z')"
    printf 'Machine: %s (%s)\n' "$1" "$(uname -m)"
    printf 'OS: '
    case "$os" in
        Darwin) printf 'macOS %s (build %s)\n' "$(sw_vers -productVersion)" "$(sw_vers -buildVersion)" ;;
        Linux) . /etc/os-release
               printf '%s, kernel %s\n' "$PRETTY_NAME" "$(uname -r)" ;;
    esac
    printf 'Toolchain versions: see below\n'
    printf 'Build settings: version queries and C++23 libc++ header preprocessing only\n'
    printf 'Commit: %s (%s)\n' "$2" "$4"
    printf 'Git dirty: %s (Mac checkout, git status --porcelain)\n' "$3"
    case "$os" in
        Darwin) printf 'CPU: %s\n' "$(sysctl -n machdep.cpu.brand_string)" ;;
        Linux) LC_ALL=C lscpu | sed -n 's/^Model name:[[:space:]]*/CPU: /p' ;;
    esac

    for tool in gcc g++ clang clang++ cmake ninja git; do
        if path=$(command -v "$tool"); then
            report_version "$path"
        else
            printf '%s: absent from PATH\n' "$tool"
        fi
    done

    case "$os" in
        Darwin)
            printf 'Active developer directory: %s\n' "$(xcode-select -p)"
            printf 'macOS SDK: %s\n' "$(xcrun --sdk macosx --show-sdk-version)"
            pkgutil --pkg-info=com.apple.pkg.CLTools_Executables 2>/dev/null |
                sed -n 's/^version: /Command Line Tools receipt: /p'
            sdk=$(xcrun --sdk macosx --show-sdk-path)
            report_llvm "$(dirname "$(xcrun --sdk macosx --find clang)")" \
                -isysroot "$sdk" -mmacosx-version-min=26.0
            for bin in /opt/homebrew/opt/llvm*/bin /usr/local/opt/llvm*/bin; do
                if [ ! -x "$bin/clang++" ]; then continue; fi
                report_llvm "$bin" -isysroot "$sdk" -mmacosx-version-min=26.0
                report_libcxx 'macOS SDK headers, deployment target 26.0' "$bin/clang++" \
                    -isysroot "$sdk" -mmacosx-version-min=26.0 -nostdinc++ -isystem "$sdk/usr/include/c++/v1"
            done
            ;;
        Linux)
            printf 'Native SDK (glibc development files and Linux UAPI headers):\n'
            dpkg-query -W libc6-dev linux-libc-dev
            for bin in /usr/lib/llvm-*/bin "$HOME"/.local/*/bin; do
                if [ -x "$bin/clang++" ]; then report_llvm "$bin"; fi
                if [ -x "$bin/cmake" ]; then report_version "$bin/cmake"; fi
            done
            ;;
    esac
}

if [ "${1:-}" = --host ]; then
    report_host dev01 "$2" "$3" 'Mac checkout, script via ssh'
else
    commit=$(git rev-parse HEAD)
    git_status=$(git status --porcelain)
    dirty=no
    if [ -n "$git_status" ]; then dirty=yes; fi
    report_host Mac "$commit" "$dirty" 'Mac checkout, script run locally'
    printf '\n'
    ssh -o BatchMode=yes -o ConnectTimeout=8 dev@dev01 \
        "cd / && sh -s -- --host '$commit' '$dirty'" < "$0"
fi
