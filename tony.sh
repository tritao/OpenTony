#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV="$ROOT/.tools/venv"
GIT_COMMON="$(git -C "$ROOT" rev-parse --path-format=absolute --git-common-dir 2>/dev/null || true)"
GHIDRA_VERSION="$(awk -F'"' '/^  version: "/ {print $2; exit}' "$ROOT/re/config/ghidra.yml")"
SHARED_PYGHIDRA="${GIT_COMMON:+$GIT_COMMON/opentony/tools/pyghidra-$GHIDRA_VERSION}"

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

if [[ ! -f "$VENV/bin/tony" ]] \
  || ! "$VENV/bin/python" -c \
    'from pathlib import Path; import sys, tony; raise SystemExit(0 if Path(tony.__file__).resolve().parents[1] == Path(sys.argv[1]).resolve() else 1)' \
    "$ROOT" >/dev/null 2>&1; then
  "$VENV/bin/python" -m pip install --disable-pip-version-check --no-input --no-deps --force-reinstall -e "$ROOT"
fi

if [[ -n "$SHARED_PYGHIDRA" ]]; then
  export PYTHONPATH="$ROOT:$SHARED_PYGHIDRA${PYTHONPATH:+:$PYTHONPATH}"
fi

exec "$VENV/bin/python" -m tony "$@"
