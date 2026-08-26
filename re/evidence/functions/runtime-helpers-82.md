# Runtime helper: parser stream copy

Status: observed

`0x004fabf0` now has an exact VC6 reconstruction. It bounds a stream copy to
the available bytes, refreshes the backing buffer when needed, copies aligned
and trailing bytes, and advances both parser cursors.

The count clamp, refill call, copy loops, cursor updates, error-free return,
and register cleanup match under `vc6-coff-text`.
