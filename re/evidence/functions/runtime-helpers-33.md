# Runtime helper cluster: fixed-point transform application

Status: observed

`0x004e85a0` is now split and byte-exact. It loads the nine signed 16-bit
transform coefficients into the shared math state, evaluates the three x87
dot products with the engine scale constant, converts them through the
fixed-point helper, and returns the output vector.

The global coefficient stores, x87 stack ordering, scale loads, and helper
calls match under `vc6-coff-text`.
