#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ ! -f "$ROOT/.tools/venv/bin/activate" ]]; then
  echo "OpenTony venv missing. Run scripts/bootstrap-ubuntu.sh first." >&2
  exit 1
fi
# This script must be sourced for the activation to affect the caller.
# shellcheck disable=SC1091
source "$ROOT/.tools/venv/bin/activate"
export GHIDRA_INSTALL_DIR="$ROOT/.tools/ghidra-12.1.3"
export WINEPREFIX="$ROOT/.tools/wineprefix"
echo "OpenTony environment active"
