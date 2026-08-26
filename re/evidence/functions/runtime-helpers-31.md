# Runtime helper cluster: zlib compression wrapper

Status: observed

`0x004e92c0` is now split and byte-exact. It allocates a 1.5x destination,
calls the original compression routine, handles destination and allocation
errors, then copies successful output into an exact-sized allocation and
returns the compressed length through the caller's output pointer.

The fixed diagnostic strings, allocator/free calls, copy loops, and return
paths match under `vc6-coff-text`.
