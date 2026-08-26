# Runtime helper cluster: XA track playback and music shutdown

Status: observed

The media helpers immediately following the matched Bink audio setup are now
split and byte-exact:

- `0x004e7310` formats the XA track name from the track and sector indices,
  logs the selected track, and releases its temporary string buffer.
- `0x004e7370` requests music shutdown, waits for the worker flag to clear
  through the original `Sleep` import, or tears down the stream immediately
  when shutdown is already acknowledged; both paths log the stop message.

The absolute media globals, imported wait call, internal teardown call, and
original branch layout match under `vc6-coff-text`.
