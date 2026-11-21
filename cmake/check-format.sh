#!/usr/bin/env bash
set -euo pipefail

git_executable=$1
clang_format=$2
"$git_executable" ls-files -z --cached --others --exclude-standard -- \
  '*.c' '*.cc' '*.cpp' '*.cxx' '*.h' '*.hh' '*.hpp' '*.hxx' '*.ipp' '*.inl' '*.tpp' '*.h.in' '*.hpp.in' |
  while IFS= read -r -d '' source; do
    # The approved upstream Arrow C ABI header is kept byte-for-byte.
    if [[ -f $source && $source != src/column/include/doqe/column/arrow/c/abi.h ]]; then
      printf '%s\0' "$source"
    fi
  done | xargs -0 "$clang_format" --dry-run -Werror --
