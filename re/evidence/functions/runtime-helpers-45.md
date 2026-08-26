# Runtime helper cluster: dual-mask state interpolation

Status: observed

`0x004f21a0` is now split and byte-exact. It interpolates the scalar and base
fields, conditionally updates vector fields under the first mask, and
conditionally updates transform fields under the second mask.

The x87 arithmetic order, two flag reads, field offsets, and register-save
layout match under `vc6-coff-text`.
