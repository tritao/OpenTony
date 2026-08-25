#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ ! -r /etc/os-release ]]; then
  echo "Cannot detect Linux distribution. See docs/TOOLING.md." >&2
  exit 1
fi
# shellcheck disable=SC1091
source /etc/os-release
case "${ID:-}" in
  ubuntu|linuxmint) exec "$ROOT/scripts/bootstrap-ubuntu.sh" "$@" ;;
  arch|endeavouros|manjaro) exec "$ROOT/scripts/bootstrap-arch.sh" "$@" ;;
  *)
    echo "No automatic bootstrap for ID=${ID:-unknown}." >&2
    echo "See docs/TOOLING.md, install equivalent system packages, then:" >&2
    echo "  python3 -m venv .tools/venv" >&2
    echo "  .tools/venv/bin/pip install -e '.[dev]'" >&2
    echo "  .tools/venv/bin/tony setup ghidra" >&2
    echo "  .tools/venv/bin/tony doctor" >&2
    exit 2
    ;;
esac
