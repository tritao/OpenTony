from tony.cli import build_parser, parse_args


def test_parser_builds():
    parser = build_parser()
    assert parser.prog == "tony"


def test_experiments_parse():
    args = build_parser().parse_args(["experiments", "list"])
    assert callable(args.func)


def test_media_tracks_parse():
    args = build_parser().parse_args(["media", "tracks"])
    assert callable(args.func)


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


def test_run_and_play_headless_parse():
    for command in ("run", "play"):
        args = parse_args([command, "--headless", "--fullscreen"])
        assert args.headless is True
        assert args.game_args == ["--fullscreen"]


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
        "inspect-hed",
        "extract-hed",
    ):
        args = build_parser().parse_args(["assets", command, "archive.PKR"])
        assert callable(args.func)
