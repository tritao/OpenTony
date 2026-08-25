from __future__ import annotations

import argparse

from . import __version__
from . import commands


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="tony", description="OpenTony reverse-engineering workflow")
    parser.add_argument("--version", action="version", version=f"OpenTony {__version__}")
    sub = parser.add_subparsers(dest="command", required=True)

    p = sub.add_parser("doctor", help="check required/optional tools")
    p.set_defaults(func=commands.doctor)

    setup = sub.add_parser("setup", help="provision project-managed tools")
    setup_sub = setup.add_subparsers(dest="setup_command", required=True)
    p = setup_sub.add_parser("ghidra", help="download pinned Ghidra and install bundled PyGhidra")
    p.set_defaults(func=commands.setup_ghidra)

    media = sub.add_parser("media", help="inspect original disc media")
    media_sub = media.add_subparsers(dest="media_command", required=True)
    p = media_sub.add_parser("identify", help="hash and identify disc image without modifying it")
    p.add_argument("path", nargs="?")
    p.add_argument("--record", action="store_true")
    p.set_defaults(func=commands.media_identify)
    p = media_sub.add_parser("list", help="probe/list image contents with 7-Zip")
    p.add_argument("path", nargs="?")
    p.set_defaults(func=commands.media_list)
    p = media_sub.add_parser("extract", help="extract image/container to a generated build directory")
    p.add_argument("path", nargs="?")
    p.add_argument("--output", default="build/disc")
    p.set_defaults(func=commands.media_extract)

    exe = sub.add_parser("exe", help="inspect installed PE executable")
    exe_sub = exe.add_subparsers(dest="exe_command", required=True)
    p = exe_sub.add_parser("identify", help="record PE identity and entry-point metadata")
    p.add_argument("path")
    p.add_argument("--record", action="store_true")
    p.set_defaults(func=commands.exe_identify)

    p = sub.add_parser("verify", help="verify recorded media/executable hashes")
    p.set_defaults(func=commands.verify)

    wine = sub.add_parser("wine", help="Wine runtime management")
    wine_sub = wine.add_subparsers(dest="wine_command", required=True)
    p = wine_sub.add_parser("init", help="initialize canonical Wine prefix")
    p.set_defaults(func=commands.wine_init)

    p = sub.add_parser("run", help="run recorded THPS2 executable under Wine")
    p.add_argument("game_args", nargs=argparse.REMAINDER)
    p.set_defaults(func=commands.run_game)

    p = sub.add_parser("debug", help="run through WineDbg GDB proxy and interactive GDB")
    p.add_argument("--port", type=int)
    p.add_argument("game_args", nargs=argparse.REMAINDER)
    p.set_defaults(func=commands.debug_game)

    ghidra = sub.add_parser("ghidra", help="generated Ghidra project operations")
    ghidra_sub = ghidra.add_subparsers(dest="ghidra_command", required=True)
    p = ghidra_sub.add_parser("rebuild", help="recreate project, analyze executable, and apply tracked symbols")
    p.set_defaults(func=commands.ghidra_rebuild)
    p = ghidra_sub.add_parser("export-functions", help="export current function inventory as JSON")
    p.add_argument("--output")
    p.set_defaults(func=commands.ghidra_export_functions)

    exp = sub.add_parser("experiments", help="experiment manifest operations")
    exp_sub = exp.add_subparsers(dest="experiments_command", required=True)
    p = exp_sub.add_parser("list")
    p.set_defaults(func=commands.experiments_list)

    p = sub.add_parser("compare", help="compare two JSONL traces and report first divergence")
    p.add_argument("left")
    p.add_argument("right")
    p.set_defaults(func=commands.compare_traces)

    return parser


def main(argv=None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    return int(args.func(args) or 0)
