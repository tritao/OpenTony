# Tooling

## Required

| Tool | Role | Policy |
|---|---|---|
| Python 3.12-3.13 | orchestration and tests | project venv under `.tools/venv` |
| Git | source/history | system package |
| Ghidra 12.1.3 | static RE | pinned + SHA-verified download |
| JDK 21 | Ghidra runtime | system package |
| PyGhidra | Python access to Ghidra API | installed from the pinned Ghidra bundle |
| Wine stable 11+ | execute THPS2 PC | WineHQ stable preferred |
| WineDbg | Win32 debugging bridge | supplied by Wine |
| GDB with Python | runtime RE | system package |
| file / 7-Zip / xorriso / libcdio | disc-image inspection | system packages |
| CMake / Ninja / Clang | future reconstruction | system packages |
| jq / ripgrep | trace/decomp inspection | system packages |

## Pinned Ghidra

`re/config/ghidra.yml` currently pins Ghidra 12.1.3 and its official SHA-256. `tony setup ghidra` downloads the release, verifies it, extracts it under `.tools/`, and installs the bundled PyGhidra package into the OpenTony venv.

We use PyGhidra's native CPython integration. Java GhidraScripts remain an escape hatch for a future custom loader/analyzer/plugin, not the default automation language.

## Wine

Wine itself is installed through the OS package manager because graphics, audio, libc, and driver integration are distribution-level concerns. The Ubuntu bootstrap installs WineHQ's stable branch.

OpenTony creates a normal 64-bit prefix at `.tools/wineprefix`. Do not force the legacy `WINEARCH=win32` model; Wine 11's new WoW64 architecture is the intended baseline.

Runtime traces should record the exact `wine --version` because Wine is not byte-pinned by this repository.

## GDB

`tony doctor` checks that GDB has embedded Python support. `tony debug` runs WineDbg's GDB proxy with a fixed local port, then connects GDB and loads `re/gdb/bootstrap.gdb`.

## Disc images

`tony media identify` never writes the image and records the detected layout. `tony media tracks` reports the ISO filesystem volume and any raw sectors beyond it as unclassified regions. `tony media list` probes the normalized filesystem without modifying the source. `tony media extract` writes to `build/` only: raw Mode 2/Form 1 CD images are converted from 2352-byte sectors to 2048-byte ISO payloads, then extracted with xorriso. Each extraction writes a manifest containing source/output hashes, sector parameters, raw-tail metadata, and tool versions.

## Not initial dependencies

Do not add these until a concrete need appears:

- Frida
- DynamoRIO
- Intel PIN
- QEMU/86Box
- PCSX-Redux
- SDL
- PartyMod or other THPS projects as Git submodules

External research can still be consulted; see `references/README.md`.

## Bootstrap coverage

`scripts/bootstrap-linux.sh` dispatches to Ubuntu/Linux Mint and Arch-family provisioning. Linux Mint uses its `UBUNTU_CODENAME` when configuring the WineHQ repository. Other distributions are intentionally not guessed: install the equivalent packages listed above, create `.tools/venv`, install OpenTony editable, run `tony setup ghidra`, then `tony doctor`.
