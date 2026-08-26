# THawk2 startup runtime anchor

Status: observed
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Runtime executable: generated `build/disc/files/SETUP/data/THawk2.nocd.exe`

## Static anchor

A fresh PyGhidra rebuild of the recorded retail executable imported the PE32/i386 image and exported `4739` functions. Ghidra identifies the PE entry point as `0x00502f74`.

## Runtime observations

Using the WineDbg GDB proxy and `tony debug`:

- Breakpoint at `0x00502f74` stopped at the executable entry point.
- The entry-point backtrace returned through `0x00503054` and Wine's thread startup helpers.
- Breakpoint at the known CD-check helper `0x004bb240` was reached from `0x00503054` during startup.
- Continuing from the CD-check helper produced the expected runtime paths (`CDPATH`, `PKRPATH`, `MUSPATH`, `MOVPATH`) and DirectDraw initialization messages.
- The normal Wine virtual-desktop run reached `PCMOVIE_PlayGameFMV`, attempted `\\NSLOGO.STR;1`, and reported a 640x480 movie with 266 frames before the debugger session was stopped.
- An isolated Xvfb launch using the old 24-bit screen stopped in DirectDraw with `DDERR_UNSUPPORTED`.
- Forcing `LIBGL_ALWAYS_SOFTWARE=1` caused `DirectDrawCreateEx` to hang. Leaving that override unset, selecting Wine's `gl` renderer, and matching both Xvfb and Wine's virtual desktop to `640x480x16` allowed `DirectDrawCreateEx` and `SetDisplayMode(640, 480, 16)` to return success.
- With that profile, the game reported `DirectX and Direct3D Are Up` headlessly. Returning immediately from the blocking movie routine at `0x004e5ec0` skipped `ATVILOGO.STR`, `NSLOGO.STR`, and `INTRO.STR`.
- Execution then stopped at `0x00452ff0`, the statically identified frontend state machine, with return address `0x00503054`. This is the first dynamically observed stable post-startup anchor.

## Interpretation

These observations establish a reproducible startup-to-frontend path for this exact executable build. The GDB bootstrap command `tony-skip-movies` installs the recorded `0x004e5ec0` bypass; `tony-bp-thps2 frontend temporary` captures the resulting frontend entry. They do not yet dynamically confirm the level loop, frame boundary, or player state.

## Setup and media file probes

The startup configuration helper at `0x004f6850` constructs a path from the
configured section and filename, then probes it through the Win32/CRT file
layer. Its callers use the configuration keys `MOVPATH` and `MUSPATH` with
the literals `Intro.dat` and `LTIX30.dat`; these are setup/media probes, not
PSX scene or gameplay-object assets. The same helper is also used for the
configured package path before the normal PKR-backed resource loads.

`GrayMat.dat` is passed at `0x0046a549` to the blocking movie path
`0x004e5ec0`, alongside the `ATVILOGO.STR`, `NSLOGO.STR`, and `INTRO.STR`
startup movies. It is therefore recorded as startup movie media rather than
as a level texture. The extracted gameplay-asset inventory contains no
separate proven runtime object family for any of these files.

## Open questions / next experiment

- Automate frontend input far enough to select Warehouse and capture `0x0046a3a0` at runtime.
- Determine whether repeated `0x0046a3a0` hits correspond to rendered gameplay frames and locate stable player-state roots.
