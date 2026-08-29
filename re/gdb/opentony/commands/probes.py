"""Domain probe command adapters and declarative probe families."""

from __future__ import annotations

import shlex

import gdb

from ..camera import (
    ActorSubmissionProbe,
    CameraCollisionProbe,
    CameraCollisionResultProbe,
    CameraEffectProbe,
    CameraModeOverrideProbe,
    CameraPointSelectProbe,
    CameraPointStateProbe,
    CameraPositionTransformProbe,
    CameraProbe,
    CameraTimingProbe,
    CameraViewportControlProbe,
    GeometryRasterReturnProbe,
    GeometrySubmissionProbe,
    TransformedVertexProbe,
    ViewProjectionPerturbProbe,
    ViewProjectionProbe,
)
from ..collision import (
    COLLISION_INIT_BOUNDARY_CASES,
    CollisionDynamicCullProbe,
    CollisionDynamicProbe,
    CollisionDynamicTransformMutationProbe,
    CollisionDynamicTransformProbe,
    CollisionFlagProbe,
    CollisionLoaderProbe,
    CollisionModelKindProbe,
    CollisionQueryInitBoundaryProbe,
    CollisionQueryProbe,
)
from ..physics import (
    GROUND_MOTION_CONTROL_WRITERS,
    GROUND_MOTION_CORRECTION_WRITERS,
    GROUND_MOTION_FRAME_RANDOM_SITES,
    GROUND_MOTION_PROFILE_WRITERS,
    GROUND_MOTION_RANDOM_SITES,
    GROUND_MOTION_THRESHOLD_RANDOM_SITES,
    MOTION_CORRECTION_ADD_SITES,
    OLLIE_LATCH_WRITERS,
    OLLIE_RANDOM_SITES,
    SHARED_RANDOM_SERVICE,
    SPECIAL_HANDLER_INFO,
    VELOCITY_DAMPING_COMPONENT_SITES,
    VELOCITY_DAMPING_RANDOM_SITES,
    AirCollisionQueryProbe,
    AirCorrectionProbe,
    GroundMotionControlWriterProbe,
    GroundMotionProducerProbe,
    GroundMotionProfileWriterProbe,
    GroundMotionRandomProbe,
    InAirHandlerProbe,
    MotionCorrectionAddProbe,
    MotionCorrectionProbe,
    MovementPhysicsProbe,
    OllieLatchProbe,
    OllieRandomProbe,
    PhysicsProbe,
    PhysicsStateRequestProbe,
    PhysicsStateWriterProbe,
    PlayerDiffProbe,
    ResponseCorrectionProbe,
    SharedRandomServiceProbe,
    SimulationTimeAccumulatorProbe,
    SimulationTimeStoreProbe,
    SpecialPhysicsHandlerProbe,
    SyntheticPhysicsStateForceProbe,
    VelocityDampingComponentProbe,
    VelocityDampingRandomProbe,
)
from ..physics import GroundMotionWriterProbe as GroundMotionCorrectionWriterProbe
from ..position import POSITION_COMMIT_CALLS, PositionCommitBreakpoint
from ..trg import Type192CommandProbe
from .common import (
    argv,
    integer,
    runtime_breakpoints,
    trace_writer,
    write,
)
from .control import TonyAnimationRequestBreakpoint


def _collision_family(*, writer, frame_provider=None, controller=None, watch_arm_factory=None):
    del frame_provider, controller, watch_arm_factory
    probe = CollisionQueryProbe(writer=writer)
    return [probe.entry, probe.return_breakpoint, AirCollisionQueryProbe(writer=writer)]


def _service_family(*, writer, frame_provider=None, controller=None, watch_arm_factory=None):
    del controller, watch_arm_factory
    return [SharedRandomServiceProbe(writer=writer, frame_provider=frame_provider)]


def _rng_family(*, writer, frame_provider=None, controller=None, watch_arm_factory=None):
    del frame_provider, controller, watch_arm_factory
    breakpoints = []
    for address, purpose in GROUND_MOTION_RANDOM_SITES.items():
        breakpoints.append(GroundMotionRandomProbe(address, purpose, writer=writer))
    for address, purpose in GROUND_MOTION_THRESHOLD_RANDOM_SITES.items():
        breakpoints.append(GroundMotionRandomProbe(address, purpose, writer=writer, player_register="ebp"))
    for address, purpose in GROUND_MOTION_FRAME_RANDOM_SITES.items():
        breakpoints.append(GroundMotionRandomProbe(address, purpose, writer=writer, player_register="ebp"))
    for address, purpose in OLLIE_RANDOM_SITES.items():
        breakpoints.append(OllieRandomProbe(address, purpose, writer=writer))
    for address, purpose in VELOCITY_DAMPING_RANDOM_SITES.items():
        breakpoints.append(VelocityDampingRandomProbe(address, purpose, writer=writer))
    for address, purpose in VELOCITY_DAMPING_COMPONENT_SITES.items():
        breakpoints.append(VelocityDampingComponentProbe(address, purpose, writer=writer))
    return breakpoints


def _animation_family(*, writer, frame_provider=None, controller=None, watch_arm_factory=None):
    del frame_provider, controller, watch_arm_factory
    return [TonyAnimationRequestBreakpoint(None, None, writer=writer)]


def _correction_family(*, writer, frame_provider=None, controller=None, watch_arm_factory=None):
    del frame_provider, controller
    breakpoints = [
        AirCorrectionProbe(writer=writer),
        ResponseCorrectionProbe(writer=writer),
        MotionCorrectionProbe(writer=writer),
    ]
    breakpoints.extend(
        MotionCorrectionAddProbe(address, site, writer=writer)
        for address, site in MOTION_CORRECTION_ADD_SITES.items()
    )
    if watch_arm_factory is not None:
        breakpoints.append(watch_arm_factory(writer))
    breakpoints.extend(
        GroundMotionCorrectionWriterProbe(address, spec, writer=writer)
        for address, spec in GROUND_MOTION_CORRECTION_WRITERS.items()
    )
    return breakpoints


def _state_family(*, writer, frame_provider=None, controller=None, watch_arm_factory=None):
    del frame_provider, controller, watch_arm_factory
    breakpoints = [
        PhysicsStateRequestProbe(writer=writer),
        PhysicsStateWriterProbe(writer=writer),
        InAirHandlerProbe(writer=writer),
    ]
    breakpoints.extend(SpecialPhysicsHandlerProbe(address, writer=writer) for address in SPECIAL_HANDLER_INFO)
    breakpoints.extend(OllieLatchProbe(address, writer=writer) for address in OLLIE_LATCH_WRITERS)
    return breakpoints


def _position_family(*, writer, frame_provider=None, controller=None, watch_arm_factory=None):
    del frame_provider, controller, watch_arm_factory
    return [
        PositionCommitBreakpoint(address, label, None, writer=writer)
        for address, label in POSITION_COMMIT_CALLS
    ]


def _timing_family(*, writer, frame_provider=None, controller=None, watch_arm_factory=None):
    del watch_arm_factory
    from .recording import (
        TonyRecordingTimerProducerDeltaReadBreakpoint,
        TonyRecordingTimerProducerOutputBreakpoint,
        TonyRecordingTimerProducerPreviousStoreBreakpoint,
        TonyRecordingTimerProducerReadBreakpoint,
    )

    return [
        TonyRecordingTimerProducerReadBreakpoint(controller),
        TonyRecordingTimerProducerDeltaReadBreakpoint(controller),
        TonyRecordingTimerProducerOutputBreakpoint(controller),
        TonyRecordingTimerProducerPreviousStoreBreakpoint(controller),
        SimulationTimeStoreProbe(writer=writer, frame_provider=frame_provider),
    ]


def _core_family(*, writer, frame_provider=None, controller=None, watch_arm_factory=None):
    del writer, frame_provider, controller, watch_arm_factory
    # Core recording boundaries are installed by recording.py itself. The
    # empty family gives scenario manifests one stable name for that base.
    return []


PROBE_FAMILIES = {
    "core": _core_family,
    "collision": _collision_family,
    "service": _service_family,
    "rng": _rng_family,
    "animation": _animation_family,
    "correction": _correction_family,
    "state": _state_family,
    "position": _position_family,
    "timing": _timing_family,
}


def build_probe_family(family: str, *, writer, frame_provider=None, controller=None, watch_arm_factory=None):
    try:
        builder = PROBE_FAMILIES[family]
    except KeyError as exc:
        raise ValueError(f"unknown probe family {family!r}") from exc
    return builder(
        writer=writer,
        frame_provider=frame_provider,
        controller=controller,
        watch_arm_factory=watch_arm_factory,
    )


class TonyPhysicsProbe(gdb.Command):
    """tony-physics-probe [COUNT] -- emit conservative dispatcher observations."""

    def __init__(self):
        super().__init__("tony-physics-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        try:
            values = shlex.split(arg)
        except ValueError as exc:
            raise gdb.GdbError(f"invalid arguments: {exc}") from exc
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-physics-probe [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = PhysicsProbe(count, writer=trace_writer())
        runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        write(f"physics probe armed {limit} at 0x{probe.address:08x}")


class TonyForcePhysicsState(gdb.Command):
    """tony-force-physics-state STATE -- inject one state before dispatch."""

    def __init__(self):
        super().__init__("tony-force-physics-state", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-force-physics-state STATE")
        if len(values) != 1:
            raise gdb.GdbError("usage: tony-force-physics-state STATE")
        state = integer(values[0])
        if state < 0 or state > 8:
            raise gdb.GdbError("STATE must be between 0 and 8")
        probe = SyntheticPhysicsStateForceProbe(state, writer=trace_writer())
        runtime_breakpoints.append(probe)
        write(f"synthetic physics state force armed for one live grounded dispatcher entry: 0 -> {state}")


class TonyCameraProbe(gdb.Command):
    """tony-camera-probe [COUNT] -- sample the per-frame camera update."""

    def __init__(self):
        super().__init__("tony-camera-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-camera-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-camera-probe [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CameraProbe(count, writer=trace_writer())
        runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        write(f"camera probe armed {limit} at 0x{probe.address:08x}")


class TonyCameraForceMode(gdb.Command):
    """tony-camera-force-mode MODE [HOLD] -- force a raw camera mode briefly."""

    def __init__(self):
        super().__init__("tony-camera-force-mode", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-camera-force-mode MODE [HOLD]")
        if len(values) > 2:
            raise gdb.GdbError("usage: tony-camera-force-mode MODE [HOLD]")
        mode = integer(values[0])
        hold_updates = integer(values[1]) if len(values) == 2 else 1
        if not 1 <= mode <= 25:
            raise gdb.GdbError("MODE must be between 1 and 25")
        if hold_updates <= 0:
            raise gdb.GdbError("HOLD must be positive")
        probe = CameraModeOverrideProbe(mode, hold_updates, writer=trace_writer())
        runtime_breakpoints.append(probe)
        write(
            f"camera mode {mode} will be written for {hold_updates} update(s), then mode 1 will be restored"
        )


class TonyCameraViewportProbe(gdb.Command):
    """tony-camera-viewport-probe [COUNT] [AFTER_FRAME] -- exercise raw viewport controls."""

    def __init__(self):
        super().__init__("tony-camera-viewport-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-camera-viewport-probe [COUNT] [AFTER_FRAME]") if arg.strip() else []
        if len(values) > 2:
            raise gdb.GdbError("usage: tony-camera-viewport-probe [COUNT] [AFTER_FRAME]")
        count = integer(values[0]) if values else 8
        start_frame = integer(values[1]) if len(values) == 2 else 0
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        if start_frame < 0:
            raise gdb.GdbError("AFTER_FRAME must be non-negative")
        probe = CameraViewportControlProbe(count, writer=trace_writer(), start_frame=start_frame)
        runtime_breakpoints.append(probe)
        runtime_breakpoints.append(probe.restore_breakpoint)
        write(
            f"camera viewport-control probe armed for {count} observations "
            f"after frame {start_frame} at 0x{probe.address:08x}"
        )


class TonyCameraTimingProbe(gdb.Command):
    """tony-camera-timing-probe [COUNT] -- sample the Q8 camera-rate producer."""

    def __init__(self):
        super().__init__("tony-camera-timing-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-camera-timing-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-camera-timing-probe [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CameraTimingProbe(count, writer=trace_writer())
        runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        write(f"camera timing probe armed {limit} at 0x{probe.address:08x}")


class TonyCameraPointSelectProbe(gdb.Command):
    """tony-camera-point-probe [COUNT] -- sample point/mode producer input."""

    def __init__(self):
        super().__init__("tony-camera-point-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-camera-point-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-camera-point-probe [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CameraPointSelectProbe(count, writer=trace_writer())
        runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        write(f"camera point-select probe armed {limit} at 0x{probe.address:08x}")


class TonyCameraPointStateProbe(gdb.Command):
    """tony-camera-point-state-probe [COUNT] -- sample post-selector state."""

    def __init__(self):
        super().__init__("tony-camera-point-state-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-camera-point-state-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-camera-point-state-probe [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CameraPointStateProbe(count, writer=trace_writer())
        runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        write(f"camera point-state probe armed {limit} at 0x{probe.address:08x}")


class TonyCameraEffectsProbe(gdb.Command):
    """tony-camera-effects-probe [COUNT] -- sample the effect producer boundary."""

    def __init__(self):
        super().__init__("tony-camera-effects-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-camera-effects-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-camera-effects-probe [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CameraEffectProbe(count, writer=trace_writer())
        runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        write(f"camera effects probe armed {limit} at 0x{probe.address:08x}")


class TonyViewProjectionProbe(gdb.Command):
    """tony-view-probe [COUNT] -- sample raw view/projection preparation state."""

    def __init__(self):
        super().__init__("tony-view-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-view-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-view-probe [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = ViewProjectionProbe(count, writer=trace_writer())
        runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        write(f"view projection probe armed {limit} at 0x{probe.address:08x}")


class TonyMovementPhysicsProbe(gdb.Command):
    """tony-movement-physics-probe [COUNT] -- log action/velocity handoff."""

    def __init__(self):
        super().__init__("tony-movement-physics-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-movement-physics-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-movement-physics-probe [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = MovementPhysicsProbe(count, writer=trace_writer())
        runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        write(f"movement physics probe armed {limit} at 0x{probe.address:08x}")


class TonySimulationTimeProbe(gdb.Command):
    """tony-simulation-time-probe [COUNT] -- trace the +0x2f44 time store."""

    def __init__(self):
        super().__init__("tony-simulation-time-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        del from_tty
        values = argv(arg, "tony-simulation-time-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-simulation-time-probe [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = SimulationTimeStoreProbe(count=count, writer=trace_writer())
        runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} stores"
        write(f"simulation-time store probe armed {limit} at 0x{probe.address:08x}")


class TonySimulationTimeAccumulatorProbe(gdb.Command):
    """tony-simulation-time-accumulator-probe [COUNT] -- trace timer output."""

    DEFAULT_COUNT = 1

    def __init__(self):
        super().__init__(
            "tony-simulation-time-accumulator-probe",
            gdb.COMMAND_BREAKPOINTS,
        )

    def invoke(self, arg, from_tty):
        del from_tty
        values = argv(arg, "tony-simulation-time-accumulator-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-simulation-time-accumulator-probe [COUNT]")
        count = integer(values[0]) if values else self.DEFAULT_COUNT
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = SimulationTimeAccumulatorProbe(count=count, writer=trace_writer())
        runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} samples"
        write(f"simulation-time accumulator probe armed {limit} at 0x{probe.address:08x}")


class TonyTimerCallbackProbe(gdb.Command):
    """tony-timer-callback-probe [COUNT] -- trace external timer delivery."""

    def __init__(self):
        super().__init__("tony-timer-callback-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        del from_tty
        values = argv(arg, "tony-timer-callback-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-timer-callback-probe [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = SimulationTimeAccumulatorProbe(count=count, writer=trace_writer())
        runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} samples"
        write(f"timer callback delivery probe armed {limit} at 0x{probe.address:08x}")


class TonyGroundMotionProbe(gdb.Command):
    """tony-ground-motion-probe [COUNT] -- log B010 producer inputs."""

    def __init__(self):
        super().__init__("tony-ground-motion-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-ground-motion-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-ground-motion-probe [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = GroundMotionProducerProbe(count, writer=trace_writer())
        runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        write(f"ground-motion producer probe armed {limit} at 0x{probe.address:08x}")


class TonyGroundMotionWriters(gdb.Command):
    """tony-ground-motion-writers [COUNT] [--correction] [--control]."""

    def __init__(self):
        super().__init__("tony-ground-motion-writers", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = (
            argv(
                arg,
                "tony-ground-motion-writers [COUNT] [--correction] [--control]",
            )
            if arg.strip()
            else []
        )
        flags = {value for value in values if value.startswith("--")}
        if not flags.issubset({"--correction", "--control"}):
            raise gdb.GdbError("usage: tony-ground-motion-writers [COUNT] [--correction] [--control]")
        counts = [value for value in values if not value.startswith("--")]
        if len(counts) > 1:
            raise gdb.GdbError("usage: tony-ground-motion-writers [COUNT] [--correction] [--control]")
        count = integer(counts[0]) if counts else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        selected = flags or {"--correction", "--control"}
        armed = []
        if "--correction" in selected:
            for address, spec in GROUND_MOTION_CORRECTION_WRITERS.items():
                probe = GroundMotionCorrectionWriterProbe(address, spec, count=count, writer=trace_writer())
                runtime_breakpoints.append(probe)
                armed.append(address)
        if "--control" in selected:
            for address, spec in GROUND_MOTION_CONTROL_WRITERS.items():
                probe = GroundMotionControlWriterProbe(address, spec, count=count, writer=trace_writer())
                runtime_breakpoints.append(probe)
                armed.append(address)
            for address, purpose in GROUND_MOTION_RANDOM_SITES.items():
                probe = GroundMotionRandomProbe(address, purpose, count=count, writer=trace_writer())
                runtime_breakpoints.append(probe)
                armed.append(address)
        limit = "until disabled" if count is None else f"for {count} hits per writer"
        groups = ", ".join(sorted(flag[2:] for flag in selected))
        write(
            f"ground-motion {groups} writer probes armed {limit}: "
            + ", ".join(f"0x{address:08x}" for address in armed)
        )


class TonyOllieRandomProbe(gdb.Command):
    """tony-ollie-random-probe [COUNT] -- trace prephysics RNG seams."""

    def __init__(self):
        super().__init__("tony-ollie-random-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        del from_tty
        values = argv(arg, "tony-ollie-random-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-ollie-random-probe [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        for address, purpose in OLLIE_RANDOM_SITES.items():
            probe = OllieRandomProbe(address, purpose, count=count, writer=trace_writer())
            runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} hits per site"
        write(f"ollie random probes armed {limit}")


class TonySharedRandomProbe(gdb.Command):
    """tony-rng-probe [COUNT] -- trace the shared 0x0048f3a0 boundary."""

    def __init__(self):
        super().__init__("tony-rng-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        del from_tty
        values = argv(arg, "tony-rng-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-rng-probe [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = SharedRandomServiceProbe(count=count, writer=trace_writer())
        runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} completed calls"
        write(f"shared 0x{SHARED_RANDOM_SERVICE:08x} service probes armed {limit}")


class TonyVelocityDampingRandomProbe(gdb.Command):
    """tony-velocity-damping-random-probe [COUNT] -- trace damping RNG."""

    def __init__(self):
        super().__init__("tony-velocity-damping-random-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        del from_tty
        values = argv(arg, "tony-velocity-damping-random-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-velocity-damping-random-probe [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        for address, purpose in VELOCITY_DAMPING_RANDOM_SITES.items():
            probe = VelocityDampingRandomProbe(address, purpose, count=count, writer=trace_writer())
            runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} hits per site"
        write(f"velocity damping random probes armed {limit}")


class TonyVelocityDampingComponentProbe(gdb.Command):
    """tony-velocity-damping-component-probe [COUNT] -- trace decay producers."""

    def __init__(self):
        super().__init__("tony-velocity-damping-component-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        del from_tty
        values = argv(arg, "tony-velocity-damping-component-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-velocity-damping-component-probe [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        for address, purpose in VELOCITY_DAMPING_COMPONENT_SITES.items():
            probe = VelocityDampingComponentProbe(address, purpose, count=count, writer=trace_writer())
            runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} hits per site"
        write(f"velocity damping component probes armed {limit}")


class TonyAirCorrectionProbe(gdb.Command):
    """tony-air-correction-probe [COUNT] -- trace in-air correction operands."""

    def __init__(self):
        super().__init__("tony-air-correction-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        del from_tty
        values = argv(arg, "tony-air-correction-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-air-correction-probe [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = AirCorrectionProbe(count=count, writer=trace_writer())
        runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} hits"
        write(f"air correction probe armed {limit}")


class TonyResponseCorrectionProbe(gdb.Command):
    """tony-response-correction-probe [COUNT] -- trace response additions."""

    def __init__(self):
        super().__init__("tony-response-correction-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        del from_tty
        values = argv(arg, "tony-response-correction-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-response-correction-probe [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = ResponseCorrectionProbe(count=count, writer=trace_writer())
        runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} hits"
        write(f"response correction probe armed {limit}")


class TonyMotionCorrectionProbe(gdb.Command):
    """tony-motion-correction-probe [COUNT] -- trace outer +0x58 output."""

    def __init__(self):
        super().__init__("tony-motion-correction-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        del from_tty
        values = argv(arg, "tony-motion-correction-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-motion-correction-probe [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = MotionCorrectionProbe(count=count, writer=trace_writer())
        runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} hits"
        write(f"motion correction probe armed {limit}")


class TonyGroundMotionProfileProbe(gdb.Command):
    """tony-ground-motion-profile-probe [COUNT] -- trace B010 profile sources."""

    def __init__(self):
        super().__init__("tony-ground-motion-profile-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-ground-motion-profile-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-ground-motion-profile-probe [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        for address, spec in GROUND_MOTION_PROFILE_WRITERS.items():
            probe = GroundMotionProfileWriterProbe(address, spec, count=count, writer=trace_writer())
            runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} hits per writer"
        addresses = ", ".join(f"0x{address:08x}" for address in GROUND_MOTION_PROFILE_WRITERS)
        write(f"ground-motion profile writer probes armed {limit}: {addresses}")


class TonyInAirProbe(gdb.Command):
    """tony-in-air-probe [COUNT] -- log candidate in-air handler entries."""

    def __init__(self):
        super().__init__("tony-in-air-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-in-air-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-in-air-probe [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = InAirHandlerProbe(count, writer=trace_writer())
        runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        write(f"in-air handler probe armed {limit} at 0x{probe.address:08x}")


class TonySpecialPhysicsProbe(gdb.Command):
    """tony-special-physics-probe [COUNT] -- log dedicated state handlers."""

    def __init__(self):
        super().__init__("tony-special-physics-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-special-physics-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-special-physics-probe [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        for address in SPECIAL_HANDLER_INFO:
            probe = SpecialPhysicsHandlerProbe(address, count=count, writer=trace_writer())
            runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations per handler"
        write(f"special physics handler probes armed {limit}")


class TonyAirCollisionProbe(gdb.Command):
    """tony-air-collision-probe [COUNT] -- log raw in-air cast results."""

    def __init__(self):
        super().__init__("tony-air-collision-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-air-collision-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-air-collision-probe [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = AirCollisionQueryProbe(count, writer=trace_writer())
        runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        write(f"air-collision probe armed {limit} at 0x{probe.address:08x}")


class TonyPhysicsStateRequestProbe(gdb.Command):
    """tony-physics-state-requests [COUNT] -- log state requests and reasons."""

    def __init__(self):
        super().__init__("tony-physics-state-requests", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-physics-state-requests [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-physics-state-requests [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = PhysicsStateRequestProbe(count, writer=trace_writer())
        runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        write(f"physics state-request probe armed {limit} at 0x{probe.address:08x}")


class TonyPhysicsStateWriterProbe(gdb.Command):
    """tony-physics-state-writers [COUNT] -- log the exact +0x30b8 store."""

    def __init__(self):
        super().__init__("tony-physics-state-writers", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-physics-state-writers [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-physics-state-writers [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = PhysicsStateWriterProbe(count, writer=trace_writer())
        runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        write(f"physics state-writer probe armed {limit} at 0x{probe.address:08x}")


class TonyOllieLatchProbe(gdb.Command):
    """tony-ollie-latch-probe [COUNT] -- log exact ollie latch PCs."""

    def __init__(self):
        super().__init__("tony-ollie-latch-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-ollie-latch-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-ollie-latch-probe [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        for address in OLLIE_LATCH_WRITERS:
            probe = OllieLatchProbe(address, count=count, writer=trace_writer())
            runtime_breakpoints.append(probe)
        pcs = ", ".join(f"0x{address:08x}" for address in OLLIE_LATCH_WRITERS)
        limit = "until disabled" if count is None else f"for {count} hits per PC"
        write(f"ollie latch probes armed {limit}: {pcs}")


class TonyViewProjectionPerturb(gdb.Command):
    """tony-view-perturb [COUNT] [--freeze] -- alternate view word 6."""

    def __init__(self):
        super().__init__("tony-view-perturb", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-view-perturb [COUNT] [--freeze]") if arg.strip() else []
        freeze = "--freeze" in values
        values = [value for value in values if value != "--freeze"]
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-view-perturb [COUNT] [--freeze]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = ViewProjectionPerturbProbe(count, writer=trace_writer(), freeze_input=freeze)
        runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        suffix = "; baseline view input frozen" if freeze else ""
        write(f"view projection perturb probe armed {limit} at 0x{probe.address:08x}{suffix}")


class TonyCameraPositionProbe(gdb.Command):
    """tony-camera-position-probe [COUNT] -- sample final camera transform inputs."""

    def __init__(self):
        super().__init__("tony-camera-position-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-camera-position-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-camera-position-probe [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CameraPositionTransformProbe(count, writer=trace_writer())
        runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        write(f"camera position transform probe armed {limit} at 0x{probe.address:08x}")


class TonyCameraCollisionProbe(gdb.Command):
    """tony-camera-collision-probe [COUNT] -- sample camera world queries."""

    def __init__(self):
        super().__init__("tony-camera-collision-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-camera-collision-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-camera-collision-probe [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CameraCollisionProbe(count, writer=trace_writer())
        runtime_breakpoints.append(probe)
        result_probe = CameraCollisionResultProbe(count, writer=trace_writer())
        runtime_breakpoints.append(result_probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        write(f"camera collision probes armed {limit} at 0x{probe.address:08x}/0x{result_probe.address:08x}")


class TonyActorSubmissionProbe(gdb.Command):
    """tony-actor-probe [COUNT] -- sample the actor/model submission pointer."""

    def __init__(self):
        super().__init__("tony-actor-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-actor-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-actor-probe [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = ActorSubmissionProbe(count, writer=trace_writer())
        runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        write(f"actor submission probe armed {limit} at 0x{probe.address:08x}")


class TonyGeometrySubmissionProbe(gdb.Command):
    """tony-geometry-probe [COUNT] -- sample the raw geometry handoff."""

    def __init__(self):
        super().__init__("tony-geometry-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-geometry-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-geometry-probe [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = GeometrySubmissionProbe(count, writer=trace_writer())
        runtime_breakpoints.append(probe)
        raster_probe = GeometryRasterReturnProbe(count, writer=trace_writer())
        runtime_breakpoints.append(raster_probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        write(
            f"geometry submission/raster probes armed {limit} at "
            f"0x{probe.address:08x}/0x{raster_probe.address:08x}"
        )


class TonyTransformedVertexProbe(gdb.Command):
    """tony-transformed-vertices [COUNT] -- sample common projected vertices."""

    def __init__(self):
        super().__init__("tony-transformed-vertices", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-transformed-vertices [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-transformed-vertices [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = TransformedVertexProbe(count, writer=trace_writer())
        runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        write(f"transformed vertex probe armed {limit} at 0x{probe.address:08x}")


class TonyPlayerDiff(gdb.Command):
    """tony-player-diff [COUNT] -- log changed player words at physics dispatch."""

    def __init__(self):
        super().__init__("tony-player-diff", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-player-diff [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-player-diff [COUNT]")
        count = integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = PlayerDiffProbe(count, writer=trace_writer())
        runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        write(f"player diff probe armed {limit} at 0x{probe.address:08x}")


class TonyPositionCommitProbe(gdb.Command):
    """tony-position-commit [COUNT] -- log stable callers of position commit."""

    DEFAULT_COUNT = 16

    def __init__(self):
        super().__init__("tony-position-commit", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-position-commit [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-position-commit [COUNT]")
        count = integer(values[0]) if values else self.DEFAULT_COUNT
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        for address, label in POSITION_COMMIT_CALLS:
            breakpoint = PositionCommitBreakpoint(address, label, count, writer=trace_writer())
            runtime_breakpoints.append(breakpoint)
        write(
            f"position-commit probe armed for {count} observations per caller "
            f"({len(POSITION_COMMIT_CALLS)} callsites)"
        )


class TonyCollisionProbe(gdb.Command):
    """tony-collision-probe [COUNT] -- log shared collision query inputs/results."""

    DEFAULT_COUNT = 32

    def __init__(self):
        super().__init__("tony-collision-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-collision-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-collision-probe [COUNT]")
        count = integer(values[0]) if values else self.DEFAULT_COUNT
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CollisionQueryProbe(count, writer=trace_writer())
        runtime_breakpoints.extend((probe.entry, probe.return_breakpoint))
        write(f"collision query probe armed for {count} completed calls")


class TonyCollisionInitBoundaryProbe(gdb.Command):
    """tony-collision-init-boundaries [COUNT] -- probe setup basis boundaries."""

    def __init__(self):
        super().__init__("tony-collision-init-boundaries", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = (
            argv(
                arg,
                "tony-collision-init-boundaries [COUNT]",
            )
            if arg.strip()
            else []
        )
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-collision-init-boundaries [COUNT]")
        count = integer(values[0]) if values else len(COLLISION_INIT_BOUNDARY_CASES)
        if count <= 0 or count > len(COLLISION_INIT_BOUNDARY_CASES):
            raise gdb.GdbError(f"COUNT must be between 1 and {len(COLLISION_INIT_BOUNDARY_CASES)}")
        probe = CollisionQueryInitBoundaryProbe(count, writer=trace_writer())
        runtime_breakpoints.extend(probe.breakpoints)
        write(f"collision init boundary probe armed for {count} completed calls")


class TonyCollisionLoaderProbe(gdb.Command):
    """tony-collision-loader-probe [COUNT] -- log zone-loader handoffs."""

    DEFAULT_COUNT = 4

    def __init__(self):
        super().__init__("tony-collision-loader-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-collision-loader-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-collision-loader-probe [COUNT]")
        count = integer(values[0]) if values else self.DEFAULT_COUNT
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CollisionLoaderProbe(count, writer=trace_writer())
        runtime_breakpoints.extend(probe.breakpoints)
        write(f"collision loader probe armed for {count} completed calls")


class TonyCollisionModelKindProbe(gdb.Command):
    """tony-collision-model-kind-probe [COUNT] -- log model/cache setup."""

    DEFAULT_COUNT = 32

    def __init__(self):
        super().__init__("tony-collision-model-kind-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-collision-model-kind-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-collision-model-kind-probe [COUNT]")
        count = integer(values[0]) if values else self.DEFAULT_COUNT
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CollisionModelKindProbe(count, writer=trace_writer())
        runtime_breakpoints.extend(probe.breakpoints)
        write(f"collision model-kind probe armed for {count} completed calls")


class TonyCollisionFlagsProbe(gdb.Command):
    """tony-collision-flags-probe [COUNT] -- log face flag decoding."""

    DEFAULT_COUNT = 32

    def __init__(self):
        super().__init__("tony-collision-flags-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-collision-flags-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-collision-flags-probe [COUNT]")
        count = integer(values[0]) if values else self.DEFAULT_COUNT
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CollisionFlagProbe(count, writer=trace_writer())
        runtime_breakpoints.extend(probe.breakpoints)
        write(f"collision flag probe armed for {count} completed calls")


class TonyCollisionDynamicProbe(gdb.Command):
    """tony-collision-dynamic-probe [COUNT] -- log linked-object face tests."""

    DEFAULT_COUNT = 32

    def __init__(self):
        super().__init__("tony-collision-dynamic-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-collision-dynamic-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-collision-dynamic-probe [COUNT]")
        count = integer(values[0]) if values else self.DEFAULT_COUNT
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CollisionDynamicProbe(count, writer=trace_writer())
        runtime_breakpoints.extend(probe.breakpoints)
        write(f"collision dynamic probe armed for {count} completed calls")


class TonyCollisionDynamicCullProbe(gdb.Command):
    """tony-collision-dynamic-cull-probe [COUNT] -- log linked broad-phase survivors."""

    DEFAULT_COUNT = 32

    def __init__(self):
        super().__init__("tony-collision-dynamic-cull-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-collision-dynamic-cull-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-collision-dynamic-cull-probe [COUNT]")
        count = integer(values[0]) if values else self.DEFAULT_COUNT
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CollisionDynamicCullProbe(count, writer=trace_writer())
        runtime_breakpoints.extend(probe.breakpoints)
        write(f"collision dynamic cull probe armed for {count} completed calls")


class TonyCollisionDynamicTransformProbe(gdb.Command):
    """tony-collision-transform-probe [COUNT] -- log the 0x0200 matrix tail."""

    DEFAULT_COUNT = 16

    def __init__(self):
        super().__init__("tony-collision-transform-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-collision-transform-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-collision-transform-probe [COUNT]")
        count = integer(values[0]) if values else self.DEFAULT_COUNT
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CollisionDynamicTransformProbe(count, writer=trace_writer())
        runtime_breakpoints.extend(probe.breakpoints)
        write(f"collision transform probe armed for {count} completed calls")


class TonyCollisionDynamicTransformMutationProbe(gdb.Command):
    """tony-collision-transform-mutate X Y Z [COUNT] -- calibrate Q12 scale."""

    DEFAULT_COUNT = 16

    def __init__(self):
        super().__init__("tony-collision-transform-mutate", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(
            arg,
            "tony-collision-transform-mutate X Y Z [COUNT]",
        )
        if len(values) not in (3, 4):
            raise gdb.GdbError("usage: tony-collision-transform-mutate X Y Z [COUNT]")
        scale = tuple(integer(value) for value in values[:3])
        if any(value < -0x8000 or value > 0x7FFF for value in scale):
            raise gdb.GdbError("X, Y, and Z must fit signed 16-bit Q12 words")
        count = integer(values[3]) if len(values) == 4 else self.DEFAULT_COUNT
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CollisionDynamicTransformMutationProbe(scale, count=count, writer=trace_writer())
        runtime_breakpoints.extend(probe.breakpoints)
        write(f"collision transform mutation probe armed for {count} completed calls with scale {scale}")


class TonyType192CommandProbe(gdb.Command):
    """tony-trg-type192-probe [COUNT] -- trace type-192 command effects."""

    DEFAULT_COUNT = 32

    def __init__(self):
        super().__init__("tony-trg-type192-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-trg-type192-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-trg-type192-probe [COUNT]")
        count = integer(values[0]) if values else self.DEFAULT_COUNT
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = Type192CommandProbe(count, writer=trace_writer())
        runtime_breakpoints.extend(probe.breakpoints)
        write(f"TRG type-192 command probe armed for {count} completed calls")
