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
- An isolated `1024x768x16` Xvfb launch with Mesa llvmpipe reached the game's 16-bit video-mode enumeration and Direct3D initialization; `tony debug` then hit both `0x00502f74` and `0x004bb240` headlessly.

## Interpretation

These observations establish a reproducible startup-to-CD-check anchor for this exact executable build. They do not yet identify the main loop, frame boundary, or player state.

## Open questions / next experiment

- Follow `0x00503054` in Ghidra to separate startup, movie playback, and game-loop initialization.
- Determine how to advance or bypass the intro movie reproducibly, then attach after startup and capture a stable idle frame.
- The headless debugger now uses the 16-bit llvmpipe profile from `re/config/wine.yml`. The remaining runtime question is how to advance or bypass the intro movie and capture a stable idle frame.
