#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if [[ ! -r /etc/os-release ]]; then
  echo "This bootstrap targets Ubuntu/Linux Mint. See docs/TOOLING.md for manual setup." >&2
  exit 1
fi
# shellcheck disable=SC1091
source /etc/os-release
case "${ID:-}" in
  ubuntu) CODENAME="${VERSION_CODENAME:-}" ;;
  linuxmint) CODENAME="${UBUNTU_CODENAME:-}" ;;
  *)
    echo "This bootstrap targets Ubuntu/Linux Mint; detected ID=${ID:-unknown}." >&2
    echo "Use docs/TOOLING.md and then run: tony doctor" >&2
    exit 1
    ;;
esac
if [[ -z "$CODENAME" ]]; then
  echo "Could not determine the Ubuntu base codename for ID=${ID:-unknown}." >&2
  echo "Use docs/TOOLING.md and then run: tony doctor" >&2
  exit 1
fi

echo "[1/5] Base development and RE packages"
sudo apt-get update
sudo apt-get install -y \
  ca-certificates curl wget gnupg git \
  python3 python3-venv python3-pip python3-dev \
  openjdk-21-jdk \
  gdb file p7zip-full xorriso libcdio-utils fuseiso \
  build-essential cmake ninja-build clang lld pkg-config \
  jq ripgrep

echo "[2/5] WineHQ stable"
sudo dpkg --add-architecture i386
sudo install -d -m 0755 /etc/apt/keyrings
sudo wget -q -O /etc/apt/keyrings/winehq-archive.key https://dl.winehq.org/wine-builds/winehq.key
sudo wget -q -O "/etc/apt/sources.list.d/winehq-${CODENAME}.sources" \
  "https://dl.winehq.org/wine-builds/ubuntu/dists/${CODENAME}/winehq-${CODENAME}.sources"
sudo apt-get update
sudo apt-get install -y --install-recommends winehq-stable

echo "[3/5] OpenTony virtual environment"
mkdir -p .tools
python3 -m venv .tools/venv
.tools/venv/bin/python -m pip install --upgrade pip setuptools wheel
.tools/venv/bin/python -m pip install -e '.[dev]'

echo "[4/5] Pinned Ghidra + matching PyGhidra"
.tools/venv/bin/tony setup ghidra

echo "[5/5] Health check"
set +e
.tools/venv/bin/tony doctor
status=$?
set -e

cat <<EOF

Bootstrap complete.

Activate with:
  source .tools/venv/bin/activate

Then place your image at game/THPS2.img and run:
  tony media identify --record
  tony verify
EOF

exit "$status"
