# Canonical reverse-engineering knowledge

Everything under `re/` should be reviewable evidence or a reproducible input to analysis.

- `config/` — binary/tool/runtime configuration
- `symbols/` — names and addresses we want reapplied to generated Ghidra projects
- `types/` — recovered type layouts (initially documentation/schema)
- `experiments/` — named runtime experiments
- `evidence/` — evidence records and confidence rules
- `notes/` — subsystem narratives and open questions
- `gdb/` — tracked debugger helpers

Generated analysis belongs in `build/`, not here.
