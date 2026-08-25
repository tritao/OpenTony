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
