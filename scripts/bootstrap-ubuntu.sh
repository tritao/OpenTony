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

sudo dpkg --add-architecture i386

echo "[1/7] Base development and RE packages"
sudo apt-get update
sudo apt-get install -y \
  ca-certificates curl wget gnupg git \
  python3 python3-venv python3-pip python3-dev \
  openjdk-21-jdk \
  gdb file p7zip-full xorriso libcdio-utils fuseiso \
  build-essential cmake ninja-build clang lld nasm pkg-config \
  jq ripgrep xvfb \
  libgl1:i386 libegl1:i386 libgl1-mesa-dri:i386 \
  libvulkan1:i386 mesa-vulkan-drivers:i386

echo "[2/7] WineHQ stable"
sudo install -d -m 0755 /etc/apt/keyrings
sudo wget -q -O /etc/apt/keyrings/winehq-archive.key https://dl.winehq.org/wine-builds/winehq.key
sudo wget -q -O "/etc/apt/sources.list.d/winehq-${CODENAME}.sources" \
  "https://dl.winehq.org/wine-builds/ubuntu/dists/${CODENAME}/winehq-${CODENAME}.sources"
sudo apt-get update
# Ubuntu's JACK1 amd64 package conflicts with the JACK2 i386 library pulled in
# by Wine through libasound2-plugins:i386. Select JACK2 for both architectures
# explicitly so APT can perform the provider transition instead of holding Wine.
sudo apt-get install -y libjack-jackd2-0:amd64 libjack-jackd2-0:i386
sudo apt-get install -y --install-recommends winehq-stable

echo "[3/7] OpenTony virtual environment"
mkdir -p .tools
python3 -m venv .tools/venv
.tools/venv/bin/python -m pip install --upgrade pip setuptools wheel
.tools/venv/bin/python -m pip install -e '.[dev]'

echo "[4/7] Pinned Ghidra + matching PyGhidra"
.tools/venv/bin/tony setup ghidra

echo "[5/7] Verified THPS2 disc image"
.tools/venv/bin/tony setup media

echo "[6/7] Pinned Visual C++ 6.0 SP3 toolchain"
.tools/venv/bin/tony setup vc6

echo "[7/7] Health check"
set +e
.tools/venv/bin/tony doctor
status=$?
set -e

cat <<EOF

Bootstrap complete.

Activate with:
  source .tools/venv/bin/activate

The verified image is at game/THPS2.img. Run:
  tony media identify --record
  tony verify
EOF

exit "$status"
