#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV="$ROOT/.tools/venv"

python_bin=""
for candidate in python3.12 python3; do
  if command -v "$candidate" >/dev/null 2>&1 \
    && "$candidate" -c 'import sys; raise SystemExit(0 if sys.version_info >= (3, 12) else 1)' \
    >/dev/null 2>&1; then
    python_bin="$(command -v "$candidate")"
    break
  fi
done

if [[ ! -x "$VENV/bin/python" ]]; then
  if [[ -z "$python_bin" ]]; then
    echo "OpenTony requires Python 3.12 or newer." >&2
    exit 1
  fi
  mkdir -p "$ROOT/.tools"
  "$python_bin" -m venv "$VENV"
fi

if ! "$VENV/bin/python" -c 'import sys; raise SystemExit(0 if sys.version_info >= (3, 12) else 1)' \
  >/dev/null 2>&1; then
  echo "The existing OpenTony virtual environment requires Python 3.12 or newer: $VENV" >&2
  exit 1
fi

if [[ ! -f "$VENV/bin/tony" ]]; then
  "$VENV/bin/python" -m pip install --disable-pip-version-check --no-input -e "$ROOT"
fi

exec "$VENV/bin/python" -m tony "$@"
