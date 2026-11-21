#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../../../.."
exec bash tests/integration/module-boundaries/run.sh "$@"
