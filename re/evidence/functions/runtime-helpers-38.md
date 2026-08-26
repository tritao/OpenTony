# Runtime helper cluster: FMV and music lifecycle wrappers

Status: observed

Three adjacent movie-control wrappers are now exact VC6 reconstructions:

- `0x004e70e0` logs and starts the configured FMV, then tail-dispatches to
  teardown or the game frame tick.
- `0x004e7140` stops music by waiting on the worker or immediately tearing
  down the stream, preserving the stop log and state clears.
- `0x004e7260` initializes playback, handles start/frame failures, installs
  the original callback, and returns the playback flag.

The fixed table accesses, callback calls, tail jumps, imported wait call, and
absolute state stores match under `vc6-coff-text`.
