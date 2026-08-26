# Runtime helper: video-mode descriptor validation

Status: observed

`0x004f79e0` now has an exact VC6 reconstruction. It bounds-checks the video
mode list, filters unsupported capability flags and dimensions, copies accepted
descriptors, logs the selected mode, and increments the list count.

The stride arithmetic, rejection branches, descriptor copy, diagnostic call,
and `ret 8` cleanup match under `vc6-coff-text`.
