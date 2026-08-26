# Runtime helper cluster: media descriptor reads and closes

Status: observed

The media descriptor operations are now exact VC6 reconstructions:

- `0x004e7ad0` validates a descriptor, dispatches XA reads through the media
  callback, or copies buffered data and advances its per-descriptor offset.
- `0x004e7bc0` closes XA handles and buffered streams, clears descriptor state,
  and preserves the invalid-descriptor diagnostic path.

The descriptor state arrays, callback calls, copy direction, and return-value
normalization match under `vc6-coff-text`.
