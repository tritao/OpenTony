# Asset explorer

OpenTony includes a local browser explorer for generated asset packages. It is deliberately part of the reverse-engineering tooling, not the future native engine.

Extract a PSX package first:

```bash
tony assets extract-psx build/assets/all-pkr/files/data/SKB1.PSX \
  --output build/assets/psx-skb1 --force
```

Start the explorer:

```bash
tony assets explore build/assets/psx-skb1 --open
```

Then open the printed local URL. The explorer reads `manifest.json` and provides:

- model inventory with OBJ wireframe previews;
- placed scene and collision mesh previews;
- texture thumbnails, converting the extractor's PPM files to PNG responses in memory;
- blockmap occupancy and metadata;
- links to the raw OBJ, PPM, and JSON files.

The server binds to `127.0.0.1:8765` by default and serves only the selected generated asset directory. Use `--host` and `--port` to change the listener. Stop it with `Ctrl-C`.
