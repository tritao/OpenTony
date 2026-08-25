# Game inputs (not tracked)

Place user-owned original material here. `.gitignore` excludes everything in this directory except this README.

Initial expected media path:

```text
game/THPS2.img
```

Installed files should go under:

```text
game/installed/
```

Do not assume the executable filename. After installation:

```bash
tony exe identify game/installed/<actual-exe> --record
```

Never patch canonical files in this directory. Copy anything used for a mutation experiment into `build/experiments/` first.
