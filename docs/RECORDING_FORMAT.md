# Recording format

OpenTony's canonical recording artifact is binary **OTREC2**.  The first
eight bytes are `OTREC2\0\0`; the versioned header contains bounded section
offsets for the initial state, frame index, snapshots, event/timer data, and
metadata.  Each frame keeps normalized before/after snapshots and, when
captured in-process, the original `0x3210` player blobs plus six-word timing
observations.  Causal events are length-delimited records (`type`, `version`,
`size`, frame, phase), so adding an event does not resize existing frames.

`tony.recording.load_recording(path)` is the format boundary.  It accepts
OTREC2, legacy JSONL `.otrec`, and fixed-layout `.otcap` transport files and
returns the same `Recording`/`RecordingFrame` model for every source.

JSONL remains an export/debug view only:

```bash
tony record dump build/scenarios/warehouse-idle/retail.otrec
tony record export-json build/scenarios/warehouse-idle/retail.otrec \
  --output build/scenarios/warehouse-idle/retail.jsonl
```

The GDB adapter still consumes that compatibility view during migration;
normal in-process scenario capture writes OTREC2 directly.
