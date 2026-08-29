# In-process capture (Windows)

The normal scenario recorder remains GDB-owned.  The first migration slice is
available explicitly with `--backend inproc`:

```text
tony scenario capture warehouse-idle --backend inproc
```

Build the standalone 32-bit Windows component from a Windows CMake generator
(the Linux/native CMake project does not include these targets):

```text
cmake -S src/capture/win32 -B build/capture/win32 -A Win32
cmake --build build/capture/win32 --config Release
```

The C hook manifest is generated from `re/config/capture_hooks.yml`; regenerate
the checked-in VC6-compatible header with
`python scripts/generate_capture_hooks.py re/config/capture_hooks.yml
src/capture/win32/capture_hooks_generated.h` after changing a seam.

The host creates a bounded 64 MiB named mapping, launches the selected retail
executable suspended, injects `opentony_capture.dll`, and saves the mapping as
`retail.otcap`.  The host never drops records: a frame-limit or mapping
overflow is a failed capture.  Python then validates the fixed wire layout and
converts it to the existing JSONL `retail.otrec` contract.

M5 installs the proven `Skater_PhysicsFrame`, post-poll action-mask, and timer
boundary detours. M7 adds the deterministic frontend bootstrap: the DLL skips
the two movie entry points, releases the real PLAY and level-select boundaries,
applies the configured level at the launch seam, and supplies the same
keyboard-edge pulses used by the GDB frontend commands. Every frontend seam is
covered by the same RVA/expected-byte manifest and is installed transactionally
with the gameplay hooks.
The installer tracks persistent recorder seams separately from one-shot
frontend bootstrap seams; the latter are restored and their trampolines freed
at the first canonical player physics entry (the action seam also runs during
frontend startup).
The DLL copies the original prologues into executable trampolines, relocates
only the `rel32` calls/jumps listed for that seam in
`re/config/capture_hooks.yml`, patches the entries with relative jumps, and
captures one `0x3210` player blob immediately before and after the original
physics function.  At the post-poll boundary it applies the same low-word
action mask that `tony-action-edge` writes, using the fixed scenario intervals
in the shared config.  The timer detour samples the callback-owned counter,
accumulators, and pause gates at the timer-update and simulation-clock-read
boundaries and stores those fixed-size observations in each frame.  A
post-physics sample is assigned to the following frame, matching the GDB
recorder's return-boundary semantics.  The offline
decoder infers completed deliveries from counter deltas (deferring a torn
callback when the simulation accumulator proves it is still in flight).  The
fixed causal-event slots remain reserved and zero-filled until a typed
service/RNG producer contract is proven; version 1 rejects non-zero entries.
The wrappers
preserve the game's ECX/stack, return-value, and flags contract.  An unknown
executable or changed byte fails closed before capture begins.

The host stops the suspended retail process after the configured frame limit is
published.  The final record is committed before `frame_count` advances, so a
bounded run cannot produce a partially written frame.
If the retail frontend never reaches the requested gameplay boundary, the
host fails closed after a bounded startup wait with `OTCAP_ERROR_TIMEOUT`
instead of leaving a Wine process running indefinitely.

The Python command always starts this host inside the project-managed Xvfb
display and creates the configured Wine virtual desktop (`OpenTony,1024x768`)
for the legacy DirectDraw frontend. The desktop is torn down after the host
publishes its bounded output, so no game window is exposed on the caller's
display. Running the Windows host directly with `wine` is a diagnostic escape
hatch and may show a game window on the caller's normal display; that does not
change the headless scenario path.

`--backend hybrid` is reserved for the same-run shadow comparator milestone;
until the GDB/in-process snapshots have been proven equivalent it exits with a
clear error rather than silently producing a non-canonical recording.

The qualification helpers are `scripts/benchmark_capture.py` for wall-clock
measurements and `scripts/compare_recorders.py --scope snapshots` (or
`tony capture qualify --gdb GDB.otrec --inproc INPROC.otrec`) for M3
same-frame before/after snapshot comparisons.  The default `tony capture
compare` scope remains an exact comparison that includes all event arrays.
