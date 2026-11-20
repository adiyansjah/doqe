#!/usr/bin/env bash
set -euo pipefail

git_executable=$1
clang_format=$2
"$git_executable" ls-files -z -- '*.c' '*.cc' '*.cpp' '*.cxx' '*.h' '*.hh' '*.hpp' '*.hxx' \
  '*.ipp' '*.inl' '*.tpp' '*.h.in' '*.hpp.in' |
  xargs -0 "$clang_format" --dry-run -Werror --
