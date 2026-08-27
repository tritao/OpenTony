"""Raw global animation-clock state used by retail frame updates."""

ANIMATION_CLOCK = 0x005685F4
ANIMATION_TIME_SCALE = 0x0056865C
ANIMATION_TIME_SCALE_SQUARE = 0x00568804
ANIMATION_CLOCK_ACCUMULATOR = 0x00568810
SIMULATION_TIME = 0x0056E320
TIMING_DELTA_Q11 = 0x0056A93C

TIMING_FIELDS = {
    "animation_clock": ANIMATION_CLOCK,
    "animation_time_scale": ANIMATION_TIME_SCALE,
    "animation_time_scale_square": ANIMATION_TIME_SCALE_SQUARE,
    "animation_clock_accumulator": ANIMATION_CLOCK_ACCUMULATOR,
    "simulation_time": SIMULATION_TIME,
    "timing_delta_q11": TIMING_DELTA_Q11,
}


def _signed32(value: int) -> int:
    return value - 0x100000000 if value & 0x80000000 else value


def timing_word(memory, address: int) -> dict[str, int]:
    raw = memory.u32(address)
    return {"raw": raw, "signed": _signed32(raw)}


def animation_timing_record(memory) -> dict[str, dict[str, int]]:
    """Capture the raw global clock values consumed by gameplay/animation."""

    return {
        name: timing_word(memory, address)
        for name, address in TIMING_FIELDS.items()
    }


def timing_raw_value(record, name: str) -> int | None:
    """Read a raw timing word from a recording snapshot."""

    value = record.get(name) if isinstance(record, dict) else None
    if not isinstance(value, dict):
        return None
    value = value.get("raw")
    if not isinstance(value, int):
        return None
    return value & 0xFFFFFFFF
