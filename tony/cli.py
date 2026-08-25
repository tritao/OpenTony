from __future__ import annotations

import argparse
import sys

from . import __version__, commands


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
    p = media_sub.add_parser("tracks", help="report filesystem and unclassified raw-disc regions")
    p.add_argument("path", nargs="?")
    p.set_defaults(func=commands.media_tracks)
    p = media_sub.add_parser("list", help="probe/list image contents")
    p.add_argument("path", nargs="?")
    p.set_defaults(func=commands.media_list)
    p = media_sub.add_parser("extract", help="extract image/container to a generated build directory")
    p.add_argument("path", nargs="?")
    p.add_argument("--output", default="build/disc")
    p.add_argument("--force", action="store_true", help="replace an existing generated output directory")
    p.set_defaults(func=commands.media_extract)

    assets = sub.add_parser("assets", help="inspect and extract game asset archives")
    assets_sub = assets.add_subparsers(dest="assets_command", required=True)
    p = assets_sub.add_parser("inventory", help="summarize extracted assets by file extension")
    p.add_argument("path")
    p.add_argument("--examples", type=int, default=3, help="number of example paths to retain per extension")
    p.set_defaults(func=commands.assets_inventory)
    p = assets_sub.add_parser("inspect-pkr", help="inspect a PKR2 asset archive")
    p.add_argument("path")
    p.add_argument("--entries", action="store_true", help="include every file entry in the JSON output")
    p.set_defaults(func=commands.assets_inspect_pkr)
    p = assets_sub.add_parser("inspect-pre", help="inspect a PRE resource archive")
    p.add_argument("path")
    p.add_argument("--entries", action="store_true", help="include every file entry in the JSON output")
    p.set_defaults(func=commands.assets_inspect_pre)
    p = assets_sub.add_parser("inspect-trg", help="inspect a TRG header and node table")
    p.add_argument("path")
    p.add_argument("--nodes", action="store_true", help="include every node offset and type")
    p.set_defaults(func=commands.assets_inspect_trg)
    p = assets_sub.add_parser("inspect-psx", help="inspect a PSX model and texture container")
    p.add_argument("path")
    p.add_argument("--models", action="store_true", help="include every model header and face flags")
    p.add_argument("--textures", action="store_true", help="include every texture header")
    p.add_argument("--tags", action="store_true", help="include every PSX tag")
    p.set_defaults(func=commands.assets_inspect_psx)
    p = assets_sub.add_parser("inspect-hed", help="inspect a CD.HED hash table and associated HET/WAD files")
    p.add_argument("path")
    p.add_argument("--names", help="HET filename table; defaults to the sibling .HET file")
    p.add_argument("--wad", help="WAD payload file; defaults to the sibling .WAD file")
    p.add_argument("--entries", action="store_true", help="include every hash/offset/size entry")
    p.set_defaults(func=commands.assets_inspect_hed)
    p = assets_sub.add_parser("extract-pkr", help="extract a PKR2 asset archive into build/")
    p.add_argument("path")
    p.add_argument("--output", default="build/assets/pkr")
    p.add_argument("--force", action="store_true", help="replace an existing generated output directory")
    p.set_defaults(func=commands.assets_extract_pkr)
    p = assets_sub.add_parser("extract-pre", help="extract a PRE resource archive into build/")
    p.add_argument("path")
    p.add_argument("--output", default="build/assets/pre")
    p.add_argument("--force", action="store_true", help="replace an existing generated output directory")
    p.set_defaults(func=commands.assets_extract_pre)
    p = assets_sub.add_parser("extract-psx", help="export PSX models as OBJ and textures as PPM")
    p.add_argument("path")
    p.add_argument("--output", default="build/assets/psx")
    p.add_argument("--force", action="store_true", help="replace an existing generated output directory")
    group = p.add_mutually_exclusive_group()
    group.add_argument("--models-only", action="store_true", help="export only OBJ model files")
    group.add_argument("--textures-only", action="store_true", help="export only PPM texture files")
    p.set_defaults(func=commands.assets_extract_psx)
    p = assets_sub.add_parser("explore", help="serve a local browser explorer for generated assets")
    p.add_argument("path", nargs="?", help="generated asset directory or asset workspace; defaults to build/assets")
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=8765)
    p.add_argument("--open", action="store_true", dest="open_browser", help="open the explorer in a browser")
    p.set_defaults(func=commands.assets_explore)
    p = assets_sub.add_parser("extract-hed", help="extract a CD.HED/CD.WAD asset table into build/")
    p.add_argument("path")
    p.add_argument("--names", help="HET filename table; defaults to the sibling .HET file")
    p.add_argument("--wad", help="WAD payload file; defaults to the sibling .WAD file")
    p.add_argument("--output", default="build/assets/cd-wad")
    p.add_argument("--force", action="store_true", help="replace an existing generated output directory")
    p.add_argument(
        "--allow-zero-wad",
        action="store_true",
        help="allow extraction from an all-zero WAD for forensic inspection",
    )
    p.set_defaults(func=commands.assets_extract_hed)

    exe = sub.add_parser("exe", help="inspect installed PE executable")
    exe_sub = exe.add_subparsers(dest="exe_command", required=True)
    p = exe_sub.add_parser("identify", help="record PE identity and entry-point metadata")
    p.add_argument("path")
    p.add_argument("--record", action="store_true")
    p.set_defaults(func=commands.exe_identify)
    p = exe_sub.add_parser("patch-nocd", help="create an adjacent executable that bypasses the CD TOC check")
    p.add_argument("--output", help="output path; defaults beside the recorded executable")
    p.set_defaults(func=commands.exe_patch_nocd)

    p = sub.add_parser("verify", help="verify recorded media/executable hashes")
    p.set_defaults(func=commands.verify)

    wine = sub.add_parser("wine", help="Wine runtime management")
    wine_sub = wine.add_subparsers(dest="wine_command", required=True)
    p = wine_sub.add_parser("init", help="initialize canonical Wine prefix")
    p.set_defaults(func=commands.wine_init)
    p = wine_sub.add_parser("mount-disc", help="mount normalized disc and map it as Wine D: CD-ROM")
    p.set_defaults(func=commands.wine_mount_disc)
    p = wine_sub.add_parser("unmount-disc", help="unmount normalized disc and remove its loop device")
    p.set_defaults(func=commands.wine_unmount_disc)

    p = sub.add_parser("run", help="run recorded THPS2 executable under Wine")
    p.add_argument("--headless", action="store_true", help="run inside the configured Xvfb software-rendering display")
    p.add_argument("--screenshot", metavar="PATH", help="capture the isolated display immediately after launch")
    p.add_argument("--record", metavar="PATH", help="record the isolated display until the game exits")
    p.add_argument("game_args", nargs=argparse.REMAINDER)
    p.set_defaults(func=commands.run_game)

    p = sub.add_parser("play", help="mount the disc if needed and run the recorded game under Wine")
    p.add_argument("--headless", action="store_true", help="run inside the configured Xvfb software-rendering display")
    p.add_argument("--screenshot", metavar="PATH", help="capture the isolated display immediately after launch")
    p.add_argument("--record", metavar="PATH", help="record the isolated display until the game exits")
    p.add_argument("game_args", nargs=argparse.REMAINDER)
    p.set_defaults(func=commands.play_game)

    p = sub.add_parser("debug", help="run through WineDbg GDB proxy and interactive GDB")
    p.add_argument("--headless", action="store_true", help="use the isolated Xvfb display (the default for launches)")
    p.add_argument("--screenshot", metavar="PATH", help="capture the isolated display after the target starts")
    p.add_argument("--record", metavar="PATH", help="record the isolated display until GDB exits")
    p.add_argument("--session", help="named debug session; omitted generates a unique session")
    p.add_argument("--port", type=int)
    p.add_argument("--pid", help="attach to a PID, or use 'auto' to find the running game")
    p.add_argument("game_args", nargs=argparse.REMAINDER)
    p.set_defaults(func=commands.debug_game)

    sessions = sub.add_parser("sessions", help="manage concurrent debug sessions")
    sessions_sub = sessions.add_subparsers(dest="sessions_command", required=True)
    p = sessions_sub.add_parser("list", help="list debug sessions")
    p.set_defaults(func=commands.sessions_list)
    p = sessions_sub.add_parser("stop", help="stop a debug session and its owned processes")
    p.add_argument("session_id")
    p.set_defaults(func=commands.sessions_stop)
    p = sessions_sub.add_parser("clean", help="remove stopped debug session metadata")
    p.add_argument("session_id")
    p.set_defaults(func=commands.sessions_clean)

    ghidra = sub.add_parser("ghidra", help="generated Ghidra project operations")
    ghidra_sub = ghidra.add_subparsers(dest="ghidra_command", required=True)
    p = ghidra_sub.add_parser("rebuild", help="recreate project, analyze executable, and apply tracked symbols")
    p.set_defaults(func=commands.ghidra_rebuild)
    p = ghidra_sub.add_parser("export-functions", help="export current function inventory as JSON")
    p.add_argument("--output")
    p.set_defaults(func=commands.ghidra_export_functions)
    p = ghidra_sub.add_parser("decompile", help="decompile one function from the local Ghidra project")
    p.add_argument("address", type=lambda value: int(value, 0), help="function entry address, for example 0x0041c2d0")
    p.add_argument("--output", help="write decompiler output instead of printing it")
    p.set_defaults(func=commands.ghidra_decompile)

    exp = sub.add_parser("experiments", help="experiment manifest operations")
    exp_sub = exp.add_subparsers(dest="experiments_command", required=True)
    p = exp_sub.add_parser("list")
    p.set_defaults(func=commands.experiments_list)

    p = sub.add_parser("compare", help="compare two JSONL traces and report first divergence")
    p.add_argument("left")
    p.add_argument("right")
    p.set_defaults(func=commands.compare_traces)

    return parser


def parse_args(argv=None) -> argparse.Namespace:
    parser = build_parser()
    raw_argv = list(sys.argv[1:] if argv is None else argv)
    args, unknown = parser.parse_known_args(argv)
    if unknown:
        if args.command not in {"run", "play", "debug"}:
            parser.error("unrecognized arguments: " + " ".join(unknown))
        args.game_args.extend(unknown)
    if args.command in {"run", "play"}:
        command_index = raw_argv.index(args.command)
        raw_game_args = raw_argv[command_index + 1:]
        args.game_args = []
        index = 0
        while index < len(raw_game_args):
            value = raw_game_args[index]
            if value == "--":
                args.game_args.extend(raw_game_args[index + 1:])
                break
            if value == "--headless":
                index += 1
                continue
            if value in {"--screenshot", "--record"}:
                index += 2
                continue
            args.game_args.append(value)
            index += 1
    if args.command in {"run", "play", "debug"} and args.game_args[:1] == ["--"]:
        del args.game_args[0]
    return args


def main(argv=None) -> int:
    args = parse_args(argv)
    return int(args.func(args) or 0)
