import json

from tony.cli import build_parser
from tony.explorer import explorer_root, parse_obj, ppm_to_png


def test_parse_obj_returns_preview_geometry():
    result = parse_obj(
        """mtllib materials.mtl
o test
v 0 0 0
v 1 0 0
v 0 1 0
usemtl surface_0000
f 1//1 2//1 3//1
"""
    )

    assert result["vertex_count"] == 3
    assert result["face_count"] == 1
    assert result["objects"] == ["test"]
    assert result["materials"] == ["surface_0000"]
    assert result["bounds"] == [0.0, 0.0, 0.0, 1.0, 1.0, 0.0]


def test_ppm_to_png_preserves_binary_pixels():
    ppm = b"P6\n2 1\n255\n" + bytes((255, 0, 0, 0, 255, 0))

    png = ppm_to_png(ppm)

    assert png.startswith(b"\x89PNG\r\n\x1a\n")
    assert png.endswith(b"IEND\xaeB`\x82")


def test_explorer_root_requires_manifest(tmp_path):
    (tmp_path / "manifest.json").write_text(json.dumps({"version": 1}), encoding="utf-8")

    assert explorer_root(tmp_path) == tmp_path.resolve()


def test_explorer_command_parses():
    args = build_parser().parse_args(["assets", "explore", "build/assets/psx", "--port", "9000", "--open"])

    assert args.path == "build/assets/psx"
    assert args.port == 9000
    assert args.open_browser is True
    assert callable(args.func)
