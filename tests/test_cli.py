from tony.cli import build_parser, parse_args


def test_parser_builds():
    parser = build_parser()
    assert parser.prog == "tony"


def test_experiments_parse():
    args = build_parser().parse_args(["experiments", "list"])
    assert callable(args.func)


def test_ghidra_decompile_parse():
    args = build_parser().parse_args(["ghidra", "decompile", "0x0041c2d0", "--output", "loop.c"])
    assert args.address == 0x0041C2D0
    assert args.output == "loop.c"
    assert callable(args.func)


def test_ghidra_iteration_commands_parse():
    rebuild = build_parser().parse_args(["ghidra", "rebuild", "--profile", "fast"])
    sync = build_parser().parse_args(["ghidra", "sync", "--function", "0x00466090", "--force"])
    verify = build_parser().parse_args(["ghidra", "verify"])

    assert rebuild.profile == "fast"
    assert sync.function == [0x00466090]
    assert sync.force is True
    assert callable(verify.func)

    inspect = build_parser().parse_args(["ghidra", "inspect", "0x004638d0", "--output", "inspect.json"])
    gaps = build_parser().parse_args(["ghidra", "gaps", "--limit", "12"])
    assert inspect.address == 0x004638D0
    assert inspect.output == "inspect.json"
    assert gaps.limit == 12


def test_gdb_generate_parse():
    args = build_parser().parse_args(["gdb", "generate", "--output", "knowledge.py"])
    assert args.output == "knowledge.py"
    assert callable(args.func)


def test_setup_media_parse():
    args = build_parser().parse_args(["setup", "media"])
    assert callable(args.func)


def test_vc6_commands_parse():
    assert callable(build_parser().parse_args(["setup", "vc6"]).func)
    assert callable(build_parser().parse_args(["vc6", "verify"]).func)
    assert callable(build_parser().parse_args(["vc6", "compile", "function.cpp"]).func)
    assert callable(build_parser().parse_args(["vc6", "compare", "text_004ca9f0"]).func)


def test_split_commands_parse():
    commands = {
        "init": [],
        "extract": [],
        "build": [],
        "symbols": [],
        "module": ["0x401000", "0x401010"],
        "compare": ["0x401000"],
        "coverage": [],
        "propose-modules": [],
        "accept-proposal": ["0x401000"],
        "accept-proposals": ["--tracked-only", "--dry-run"],
        "rebuild": [],
        "verify": [],
    }
    for command, operands in commands.items():
        args = build_parser().parse_args(["split", command, *operands])
        assert callable(args.func)


def test_media_tracks_parse():
    args = build_parser().parse_args(["media", "tracks"])
    assert callable(args.func)


def test_native_verify_parse():
    assert callable(build_parser().parse_args(["native", "verify"]).func)


def test_wine_disc_commands_parse():
    for command in ("mount-disc", "unmount-disc"):
        args = build_parser().parse_args(["wine", command])
        assert callable(args.func)


def test_nocd_patch_command_parse():
    args = build_parser().parse_args(["exe", "patch-nocd"])
    assert callable(args.func)


def test_debug_pid_parse():
    args = build_parser().parse_args(["debug", "--pid", "1234"])
    assert args.pid == "1234"
    assert callable(args.func)


def test_debug_pid_auto_parse():
    args = build_parser().parse_args(["debug", "--pid", "auto"])
    assert args.pid == "auto"
    assert callable(args.func)


def test_debug_session_parse():
    args = build_parser().parse_args(["debug", "--session", "warehouse", "--port", "31340"])
    assert args.session == "warehouse"
    assert args.port == 31340
    assert callable(args.func)


def test_debug_unmute_parse():
    args = build_parser().parse_args(["debug", "--unmute"])
    assert args.unmute is True


def test_sessions_commands_parse():
    for command in ("list", "stop", "clean"):
        argv = ["sessions", command]
        if command != "list":
            argv.append("warehouse")
        args = build_parser().parse_args(argv)
        assert callable(args.func)


def test_run_and_play_headless_parse():
    for command in ("run", "play"):
        args = parse_args([command, "--headless", "--fullscreen"])
        assert args.headless is True
        assert args.game_args == ["--fullscreen"]


def test_visual_options_are_not_forwarded_to_game():
    args = parse_args(["run", "--headless", "--screenshot", "frame.png", "--record", "run.mp4", "--fullscreen"])

    assert args.screenshot == "frame.png"
    assert args.record == "run.mp4"
    assert args.game_args == ["--fullscreen"]


def test_debug_visual_options_parse():
    args = parse_args(["debug", "--headless", "--screenshot", "frame.png", "--record", "run.mp4"])

    assert args.headless is True
    assert args.screenshot == "frame.png"
    assert args.record == "run.mp4"
    assert args.game_args == []


def test_headless_game_argument_is_not_consumed():
    args = parse_args(["play", "--", "--headless"])
    assert args.headless is False
    assert args.game_args == ["--headless"]


def test_asset_commands_parse():
    for command in (
        "inspect-pkr",
        "extract-pkr",
        "inspect-pre",
        "extract-pre",
        "inventory",
        "inspect-trg",
        "inspect-psx",
        "extract-psx",
        "inspect-hed",
        "extract-hed",
    ):
        args = build_parser().parse_args(["assets", command, "archive.PKR"])
        assert callable(args.func)

    args = build_parser().parse_args(["assets", "inspect-trg", "archive.TRG", "--scripts"])
    assert args.scripts is True


def test_types_verify_command_parse():
    args = build_parser().parse_args(["types", "verify"])
    assert callable(args.func)
