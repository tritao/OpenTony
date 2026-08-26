# Runtime helper cluster: viewport extent normalization

Status: observed

`0x004e87f0` is now split and byte-exact. It copies four signed 16-bit
viewport extents into the render state, stamps the original state tag, and
normalizes zero height or depth values to one. The fixed field offsets and
zero checks match under `vc6-coff-text`.
