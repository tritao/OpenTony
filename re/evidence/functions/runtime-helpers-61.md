# Renderer-state transition helper

Status: observed

`0x004f6d20` now has an exact VC6 reconstruction. It handles renderer mode
transitions, releases and reacquires the two device interfaces when required,
updates the active-level state, and publishes the renderer status through the
existing diagnostic path.

The vtable calls, absolute renderer globals, level-range branches, and cleanup
paths match under `vc6-coff-text`.
