# Runtime helper: DirectDraw device record

Status: observed

`0x004f8630` now has an exact VC6 reconstruction. It bounds-checks the
DirectDraw device list, copies or clears the four-word descriptor, allocates and
copies the device name, logs the resulting pointer, and increments the list
count.

The null-descriptor branch, record stride, allocation call, diagnostic call,
and `ret 0x14` cleanup match under `vc6-coff-text`.
