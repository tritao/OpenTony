# Runtime helper cluster: fixed-point vector normalization

Status: observed

`0x004e8480` is now split and byte-exact. It computes the fixed-point vector
magnitude with the original x87 sequence, converts through the engine's
fixed-point helper, and writes normalized integer components to the result
vector on both the positive and fallback paths.

The x87 stack order, scale constant, helper calls, and branch-specific stores
match under `vc6-coff-text`.
