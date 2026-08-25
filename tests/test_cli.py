from tony.cli import build_parser


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


def test_asset_commands_parse():
    for command in (
        "inspect-pkr",
        "extract-pkr",
        "inspect-pre",
        "extract-pre",
        "inventory",
        "inspect-trg",
        "inspect-hed",
        "extract-hed",
    ):
        args = build_parser().parse_args(["assets", command, "archive.PKR"])
        assert callable(args.func)
