#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if [[ ! -r /etc/os-release ]]; then
  echo "Cannot detect distribution." >&2
  exit 1
fi
# shellcheck disable=SC1091
source /etc/os-release
case "${ID:-}" in
  arch|endeavouros|manjaro) ;;
  *)
    echo "This bootstrap targets Arch-family systems; detected ID=${ID:-unknown}." >&2
    exit 1
    ;;
esac

echo "[1/4] System packages"
sudo pacman -Syu --needed --noconfirm \
  base-devel git ca-certificates curl wget unzip \
  python python-pip \
  jdk21-openjdk \
  gdb wine p7zip file xorriso libcdio fuseiso \
  cmake ninja clang lld pkgconf jq ripgrep

echo "[2/4] OpenTony virtual environment"
mkdir -p .tools
python -m venv .tools/venv
.tools/venv/bin/python -m pip install --upgrade pip setuptools wheel
.tools/venv/bin/python -m pip install -e '.[dev]'

echo "[3/4] Pinned Ghidra + matching PyGhidra"
.tools/venv/bin/tony setup ghidra

echo "[4/4] Health check"
set +e
.tools/venv/bin/tony doctor
status=$?
set -e

cat <<EOF

Bootstrap complete.
Activate with:
  source .tools/venv/bin/activate
EOF
exit "$status"
