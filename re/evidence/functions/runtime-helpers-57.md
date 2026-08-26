# Runtime helper cluster: fixed-point products

Status: observed

Four compact math helpers are now exact VC6 reconstructions:

- `0x004f5f10` divides a fixed-point product.
- `0x004f5f50` combines two products and returns the shared conversion
  polarity.
- `0x004f5f90` computes a three-component fixed-point dot product.
- `0x004f5fc0` scales one fixed-point component.

The x87 product/addition sequences, absolute scale load, conversion calls, and
tail jumps match under `vc6-coff-text`.
