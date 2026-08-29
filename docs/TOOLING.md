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
| Visual C++ 6.0 SP3 | matching retail compiler/linker | pinned media extracted under `.tools/vc6` and run with Wine |
| jq / ripgrep | trace/decomp inspection | system packages |

## Pinned Ghidra

`re/config/ghidra.yml` currently pins Ghidra 12.1.3 and its official SHA-256. `tony setup ghidra` downloads the release, verifies it, extracts it under `.tools/`, and installs the bundled PyGhidra package into the OpenTony venv.

We use PyGhidra's native CPython integration. Java GhidraScripts remain an escape hatch for a future custom loader/analyzer/plugin, not the default automation language.

## Wine

Wine itself is installed through the OS package manager because graphics, audio, libc, and driver integration are distribution-level concerns. The Ubuntu bootstrap installs WineHQ's stable branch.

OpenTony creates a normal 64-bit prefix at `.tools/wineprefix`. Do not force the legacy `WINEARCH=win32` model; Wine 11's new WoW64 architecture is the intended baseline.

The configured `tony run`/`tony play` path uses a `1024x768` Wine virtual desktop. This keeps old DirectDraw mode changes inside a Wine window and prevents the game from changing the host display mode. The size is configured in `re/config/wine.yml`.

New `tony debug` launches are isolated in Xvfb by default; use `tony debug --pid <pid>` to attach to a visible game launched by `tony run` or `tony play`.
Debug sessions are muted by default: `tony debug` routes game audio to a temporary
PulseAudio null sink and removes it when the session ends. Sink ownership is stored
in the session metadata so `tony sessions stop` and `tony sessions clean` can retry
cleanup after an interrupted debugger. Use `tony debug --unmute` when audio is
needed. If `pactl` is unavailable, the session continues without the forced mute.
This applies to debugger-launched games; attaching with `--pid` does not change
audio for an already-running game.

Debug session metadata and disposable Wine prefixes are shared across Git
worktrees. A normally stopped debugger removes its isolated Wine prefix while
retaining the small session record and trace outputs. Use `tony sessions prune
--dry-run` to audit legacy prefixes, then `tony sessions prune` to remove stale
prefixes across all worktrees; prefixes referenced by live processes are always
preserved.

For headless smoke tests, Xvfb provides a completely separate display:

```bash
tony play --headless
```

Launch through the real frontend directly into a level with a known name or
its numeric index:

```bash
tony play --level warehouse
tony debug --level 12
```

Level launches use the same frontend automation as retail replay; numeric
indices from `0` through `12` are accepted, and the currently named aliases
are `hangar` (`0`) and `warehouse` (`12`). `tony play --level` is visible by
default like regular play; add `--headless` for an isolated Xvfb launch.

The game is rendered into that display even though it is not shown on the host desktop. OpenTony prints the temporary `DISPLAY` and `XAUTHORITY` values, and can capture the frame or record the session:

```bash
tony play --headless --screenshot build/debug/launch.png
tony debug --screenshot build/debug/debug.png --record build/debug/debug.mp4
```

Screenshots use ffmpeg when available (including the configured Xvfb dimensions) and otherwise ImageMagick's `import`; recordings require ffmpeg. Capture paths are never overwritten automatically. `tony debug` applies this 16-bit llvmpipe profile automatically for isolated launches.
Use `tony run --headless` when the disc is already mounted. The equivalent manual wrapper is:

```bash
xvfb-run -a -s "-screen 0 1024x768x16 +extension GLX" \
  env LIBGL_ALWAYS_SOFTWARE=1 MESA_LOADER_DRIVER_OVERRIDE=llvmpipe tony run
```

The PC executable is 32-bit and needs matching graphics libraries. On Ubuntu/Linux Mint, install them with:

```bash
sudo dpkg --add-architecture i386
sudo apt-get update
sudo apt-get install -y libgl1:i386 libegl1:i386 libgl1-mesa-dri:i386 libvulkan1:i386 mesa-vulkan-drivers:i386
```

The Ubuntu bootstrap installs these automatically. The Arch bootstrap installs the corresponding `lib32-libglvnd`, `lib32-mesa`, and `lib32-vulkan-icd-loader` packages.

Runtime traces should record the exact `wine --version` because Wine is not byte-pinned by this repository.

## Retail recordings and replay

The GDB recorder starts and stops only at the canonical gameplay-frame boundary.
Use the host commands while a named debug session is active:

```bash
tony record start --session warehouse --output build/recordings/retail/run.otrec
tony record stop --session warehouse
tony record validate build/recordings/retail/run.otrec
```

For a bounded corpus, add `--frames COUNT`; the recorder closes after that
many complete physics-frame returns, so the footer count is exact:

```bash
tony record start --session warehouse \
  --output build/recordings/retail/warehouse-idle-256.otrec \
  --frames 256
```

Normal recordings install only the input, timer-boundary, and canonical
player-frame capture boundaries. If the first strict replay divergence needs
more evidence, arm only the relevant forensic family in the GDB session before
starting the recapture, for example:

```text
tony-record-forensic collision
tony-record-forensic clear
```

Available families are `collision`, `service`, `rng`, `animation`,
`correction`, `state`, `position`, `timing`, and `all`. The selected probes
append events to the same recording; they are not installed by default.

Retail self-replay uses the same Warehouse frontend path, injects held keys
before the retail action-mask builder, and compares raw player state at the
physics-frame boundary:

```bash
tony replay retail build/recordings/retail/run.otrec
```

The command stops at the first divergence and reports its frame, stage, and
field. A divergent result is evidence about the next missing deterministic
channel; it is not treated as a successful parity run.

The native adapter consumes the same recording inputs through the portable
`GameplaySession`, writes a generated JSONL trace under `build/parity/`, and
compares the captured player boundary fields against the retail golden:

```bash
cmake --build build/native --target opentony_native_replay
tony replay native build/recordings/retail/run.otrec
```

Native divergence is expected until the corresponding behavior is recovered;
the command reports only the first captured field that differs.

## Matching Visual C++ toolchain

`tony setup vc6` downloads and verifies the English Visual Studio 6.0 Professional base ISO and Visual Studio 6.0 Service Pack 3 ISO, extracts the command-line `VC98` toolchain under `.tools/vc6`, overlays the SP3 updates, and initializes a dedicated `.tools/vc6-prefix`. It does not modify or reuse the game Wine prefixes.

Provisioning verifies `CL.EXE` version `12.00.8168` and `LINK.EXE` version `6.00.8447`, then compiles a probe executable. These versions match the compiler and linker evidence recorded in the retail `THawk2.exe`. Run the step independently or verify an existing installation with:

```bash
tony setup vc6
tony vc6 verify
tony vc6 compile match/cpp/Math_Vector3Add.cpp
tony vc6 compare text_004ca9f0
```

The archived Microsoft media remains subject to its original licensing; availability from the configured archive does not grant a license.

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
