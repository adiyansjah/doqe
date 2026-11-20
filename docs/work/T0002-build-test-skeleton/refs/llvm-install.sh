#!/bin/bash
# Date: 2026-11-20 (WIB)
# Machines: Skyfall.local (macOS 26.6.2 arm64), dev01 (Debian 13.7 x86_64)
# Toolchain: upstream LLVM 23.1.5, release binaries
# Build settings: C++23, pinned libc++ headers and archives, macOS deployment target 26.0
# Commit: 3aaa8f483d500e40b7dadbe95491b16dfe14218b; git dirty: yes (branch t0002-build-test-skeleton)
#
# From the repo root, run the requested host mode on each host:
#   bash docs/work/T0002-build-test-skeleton/refs/llvm-install.sh mac install
#   bash docs/work/T0002-build-test-skeleton/refs/llvm-install.sh mac check
#   ssh -o BatchMode=yes dev@dev01 bash -s -- linux install < docs/work/T0002-build-test-skeleton/refs/llvm-install.sh
#   ssh -o BatchMode=yes dev@dev01 bash -s -- linux check < docs/work/T0002-build-test-skeleton/refs/llvm-install.sh
# Repeat 'check' to inspect an existing pin without replacing it.
# To resume with a downloaded archive already in scratch, pass its absolute path after
# 'install'. The script verifies it again. Delete that input after a successful install.

set -euo pipefail
export LC_ALL=C

host=${1:?usage: llvm-install.sh mac|linux install|check}
action=${2:?usage: llvm-install.sh mac|linux install|check}
downloaded_asset=${3:-}
version=23.1.5
pin="$HOME/opt/llvm-$version"
release="https://github.com/llvm/llvm-project/releases/download/llvmorg-$version"

case "$host:$(uname -s):$(uname -m)" in
    mac:Darwin:arm64)
        asset="LLVM-$version-macOS-ARM64.tar.xz"
        sha256=aba592b4a15e8f90c634c5367b51ec8418b8bcd2829865db331a17dc9418dcff
        scratch="$HOME/Downloads"
        sw_vers
        ;;
    linux:Linux:x86_64)
        asset="LLVM-$version-Linux-X64.tar.xz"
        sha256=f72edb0b36580ece7f52e23858f00c18fad4e87395ea7ad05d7e55c39f8fc78a
        scratch=/tmp
        cat /etc/debian_version
        uname -sr
        ;;
    *) printf 'Host mode and platform do not match\n' >&2; exit 1 ;;
esac
printf 'Host %s (%s)\n' "$(hostname)" "$(uname -m)"

work=$(mktemp -d "$scratch/doqe-llvm.XXXXXXXX")
stage=
cleanup() {
    rm -rf -- "$work"
    if [ -n "$stage" ]; then
        rm -rf -- "$stage"
    fi
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

case "$action" in
    install)
        if [ -e "$pin" ] || [ -L "$pin" ]; then
            printf 'Existing destination: %s. Use check to inspect it.\n' "$pin" >&2
            exit 1
        fi
        cd "$work"
        if [ -n "$downloaded_asset" ]; then
            printf 'Using downloaded archive %s\n' "$downloaded_asset"
            ln -- "$downloaded_asset" "$asset"
        else
            printf 'Downloading %s\n' "$asset"
            curl --fail --silent --show-error --location --retry 3 --output "$asset" "$release/$asset"
        fi
        printf '%s  %s\n' "$sha256" "$asset" > SHA256SUMS
        if [ "$host" = mac ]; then
            shasum -a 256 -c SHA256SUMS
        else
            sha256sum -c SHA256SUMS
        fi
        printf 'SHA-256 verified: %s\n' "$sha256"
        signer=FFB3368980F3E6BB5737145A316C56D064CACBA5
        if command -v gpg >/dev/null || command -v sq >/dev/null; then
            curl -fsSL --retry 3 -o "$asset.sig" "$release/$asset.sig"
            curl -fsSL --retry 3 -o release-keys.asc https://releases.llvm.org/release-keys.asc
            if command -v gpg >/dev/null; then
                mkdir -m 700 "$work/gnupg"
                gpg --batch --homedir "$work/gnupg" --import release-keys.asc
                gpg --batch --homedir "$work/gnupg" --status-fd 1 --verify "$asset.sig" "$asset" > signature-status
                grep -F "[GNUPG:] VALIDSIG $signer " signature-status
            else
                sq --home "$work/sq-home" cert import release-keys.asc > sq-import.log 2>&1
                sq --home "$work/sq-home" verify \
                    --signer "$signer" --signature-file "$asset.sig" "$asset"
            fi
            printf 'Detached OpenPGP signature verified: %s\n' "$signer"
        else
            printf 'Signature not verified: no local OpenPGP verifier (gpg or sq) available\n'
        fi
        mkdir -p "$HOME/opt"
        stage=$(mktemp -d "$HOME/opt/.llvm-$version.XXXXXXXX")
        printf 'Extracting %s\n' "$asset"
        tar -xJf "$asset" -C "$stage" --strip-components=1
        test -x "$stage/bin/clang++"
        if [ "$host" = mac ]; then
            xattr -lr "$stage" > "$work/xattrs"
            if grep -q 'com.apple.quarantine:' "$work/xattrs"; then
                xattr -dr com.apple.quarantine "$stage"
                printf 'Cleared macOS quarantine attributes\n'
            else
                printf 'No macOS quarantine attributes found\n'
            fi
        fi
        mv "$stage" "$pin"
        stage=
        rm -- "$asset"
        printf 'Installed %s and removed %s\n' "$pin" "$asset"
        exit 0
        ;;
    check) ;;
    *) printf 'Unknown action: %s\n' "$action" >&2; exit 1 ;;
esac

cd "$work"
for tool in clang++ clang-tidy clang-format; do
    printf '%s --version\n' "$pin/bin/$tool"
    "$pin/bin/$tool" --version | tee "$tool.version"
    grep -Eq 'version 23\.1\.5([^0-9.]|$)' "$tool.version"
done
du -sh "$pin"

find "$pin/lib" \( -name libc++.a -o -name libc++abi.a -o -name libunwind.a \) -print | sort > archives
find "$pin" \( -name '*clang_rt.fuzzer*' -o -name '*clang_rt.asan*' -o -name '*clang_rt.ubsan*' \) \
    -print | sort > runtimes

locate_archive() {
    local archive="$1" path
    path=$(grep -F "/$archive" archives || true)
    if [ ! -f "$path" ]; then
        printf 'Expected one usable %s, found: %s\n' "$archive" "$path" >&2
        return 1
    fi
    printf '%s\n' "$path"
}

printf 'Static archive paths:\n'
cat archives
printf 'Compiler-rt paths:\n'
cat runtimes
inventory_ok=1
for runtime in fuzzer asan ubsan; do
    if ! grep -qF "clang_rt.$runtime" runtimes; then
        printf 'Compiler-rt %s: missing\n' "$runtime"
        inventory_ok=0
    fi
done

headers="$pin/include/c++/v1"
printf 'libc++ headers: %s\n' "$headers"
test -f "$headers/expected"
test -f "$headers/bit"

libcxx=$(locate_archive libc++.a)
libcxxabi=$(locate_archive libc++abi.a)
flags=(-std=c++23 -stdlib=libc++ -nostdinc++ -isystem "$headers" -nostdlib++)
if [ "$host" = mac ]; then
    sdk=$(xcrun --show-sdk-path)
    printf 'macOS SDK: %s (%s)\n' "$sdk" "$(xcrun --show-sdk-version)"
    flags+=(-isysroot "$sdk" -mmacosx-version-min=26.0 -Werror=unguarded-availability)
    libraries=("$libcxx" "$libcxxabi" "-Wl,-map,smoke.map")
else
    triple_headers="$pin/include/x86_64-unknown-linux-gnu/c++/v1"
    if [ -d "$triple_headers" ]; then
        printf 'Target-specific libc++ headers: %s\n' "$triple_headers"
        flags+=(-isystem "$triple_headers")
    fi
    unwind=$(locate_archive libunwind.a)
    flags+=(--rtlib=compiler-rt --unwindlib=none)
    libraries=("-Wl,--start-group" "$libcxx" "$libcxxabi" "$unwind" "-Wl,--end-group")
fi

cat > smoke.cpp <<'CPP'
#include <bit>
#include <cstdio>
#include <expected>
#include <stdexcept>
#include <string_view>

int main() {
    std::expected<unsigned, int> value{0x12345678u};
    try {
        if (std::byteswap(value.value()) != 0x78563412u) {
            return 1;
        }
        throw std::runtime_error("static libc++ smoke");
    } catch (const std::runtime_error& error) {
        if (std::string_view(error.what()) != "static libc++ smoke") {
            return 2;
        }
        std::puts("PASS: std::expected, std::byteswap, and throw/catch");
        return 0;
    }
}
CPP

printf 'Compile:'
printf ' %q' "$pin/bin/clang++" "${flags[@]}" smoke.cpp "${libraries[@]}" -o smoke
printf '\n'
"$pin/bin/clang++" "${flags[@]}" smoke.cpp "${libraries[@]}" -o smoke
./smoke
if [ "$host" = mac ]; then
    otool -L ./smoke | tee linkage
    xcrun nm -m ./smoke > symbols
    grep -E '[[:space:]]_+Unwind_RaiseException \(from (libSystem|libunwind)\)' symbols
    grep -F 'Unwind_RaiseException' smoke.map || true
    unwind_stub="$sdk/usr/lib/system/libunwind.tbd"
    grep -m 1 -F '/usr/lib/system/libunwind.dylib' "$sdk/usr/lib/libSystem.tbd"
    grep -m 1 -F '__Unwind_RaiseException' "$unwind_stub"
else
    ldd ./smoke | tee linkage
fi

if grep -E 'libc\+\+|libgcc_s|libunwind|not found' linkage; then
    printf 'Unexpected shared runtime dependency\n' >&2
    exit 1
fi
printf 'PASS: shared dependency check\n'
test "$inventory_ok" = 1
