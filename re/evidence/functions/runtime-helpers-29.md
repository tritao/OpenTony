# Runtime helper cluster: rotation matrix constructors

Status: observed

Three 3D transform constructors are now split and byte-exact:

- `0x004e80e0` initializes an identity-scaled matrix and applies the engine's
  Y/X/Z angle order.
- `0x004e8140` applies the alternate Z/Y/X order.
- `0x004e81a0` applies the default order used by the surrounding transform
  code.

All three preserve the fixed-point `0x1000` identity values, signed angle
loads, helper call order, and returned matrix pointer under `vc6-coff-text`.
