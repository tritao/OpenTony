"""Explicit GDB command registration."""

from __future__ import annotations

from ..memory import mem
from .common import write
from .control import (
    TonyActionEdge,
    TonyActionSequence,
    TonyAnimationRequestSample,
    TonyAnimationSample,
    TonyAnimationSelectorSample,
    TonyInputSample,
    TonyJumpEdge,
    TonyKeyClear,
    TonyKeyLoop,
    TonyPlayerSample,
)
from .diagnostics import (
    TonyAddresses,
    TonyBreakpointCommand,
    TonyDiff,
    TonyDump,
    TonyFrameClock,
    TonyHexdump,
    TonyModules,
    TonyReadFloat,
    TonyReadInteger,
    TonySnapshot,
    TonyTHPS2Breakpoint,
    TonyTraceClose,
    TonyTraceOpen,
    TonyWatch,
    TonyWatchBatch,
    TonyWatchClear,
    TonyWatchLog,
    TonyWatchOnce,
)
from .probes import (
    TonyActorSubmissionProbe,
    TonyAirCollisionProbe,
    TonyAirCorrectionProbe,
    TonyCameraCollisionProbe,
    TonyCameraEffectsProbe,
    TonyCameraForceMode,
    TonyCameraPointSelectProbe,
    TonyCameraPointStateProbe,
    TonyCameraPositionProbe,
    TonyCameraProbe,
    TonyCameraTimingProbe,
    TonyCameraViewportProbe,
    TonyCollisionDynamicCullProbe,
    TonyCollisionDynamicProbe,
    TonyCollisionDynamicTransformMutationProbe,
    TonyCollisionDynamicTransformProbe,
    TonyCollisionFlagsProbe,
    TonyCollisionInitBoundaryProbe,
    TonyCollisionLoaderProbe,
    TonyCollisionModelKindProbe,
    TonyCollisionProbe,
    TonyForcePhysicsState,
    TonyGeometrySubmissionProbe,
    TonyGroundMotionProbe,
    TonyGroundMotionProfileProbe,
    TonyGroundMotionWriters,
    TonyInAirProbe,
    TonyMotionCorrectionProbe,
    TonyMovementPhysicsProbe,
    TonyOllieLatchProbe,
    TonyOllieRandomProbe,
    TonyPhysicsProbe,
    TonyPhysicsStateRequestProbe,
    TonyPhysicsStateWriterProbe,
    TonyPlayerDiff,
    TonyPositionCommitProbe,
    TonyResponseCorrectionProbe,
    TonySharedRandomProbe,
    TonySimulationTimeAccumulatorProbe,
    TonySimulationTimeProbe,
    TonySpecialPhysicsProbe,
    TonyTimerCallbackProbe,
    TonyTransformedVertexProbe,
    TonyType192CommandProbe,
    TonyVelocityDampingComponentProbe,
    TonyVelocityDampingRandomProbe,
    TonyViewProjectionPerturb,
    TonyViewProjectionProbe,
)
from .recording import (
    TonyRecordingForensic,
    TonyRecordingStart,
    TonyRecordingStatus,
    TonyRecordingStop,
    TonyRecordingToggle,
    install_recording_instrumentation,
)
from .replay import TonyRetailReplay
from .sessions import (
    TonyForceLevel,
    TonyFrontendConfirm,
    TonyFrontendPlay,
    TonySkipMovies,
)

_registered = False


def register_commands() -> None:
    global _registered
    if _registered:
        return
    TonyReadInteger("tony-read8", mem.u8, "uint8")
    TonyReadInteger("tony-read16", mem.u16, "uint16")
    TonyReadInteger("tony-read32", mem.u32, "uint32")
    TonyReadFloat()
    TonyHexdump()
    TonyDump()
    TonySnapshot()
    TonyDiff()
    TonyModules()
    TonyBreakpointCommand()
    TonyAddresses()
    TonyTHPS2Breakpoint()
    TonySkipMovies()
    TonyForceLevel()
    TonyFrontendPlay()
    TonyFrontendConfirm()
    TonyRecordingStart()
    TonyRecordingStop()
    TonyRecordingToggle()
    TonyRecordingStatus()
    TonyRecordingForensic()
    TonyRetailReplay()
    TonyPlayerSample()
    TonyInputSample()
    TonyActionEdge()
    TonyJumpEdge()
    TonyKeyLoop()
    TonyKeyClear()
    TonyAnimationSample()
    TonyAnimationRequestSample()
    TonyAnimationSelectorSample()
    TonyActionSequence()
    TonyWatch()
    TonyWatchOnce()
    TonyWatchBatch()
    TonyWatchLog()
    TonyWatchClear()
    TonyTraceOpen()
    TonyTraceClose()
    TonyFrameClock()
    TonySimulationTimeProbe()
    TonySimulationTimeAccumulatorProbe()
    TonyTimerCallbackProbe()
    TonyPhysicsProbe()
    TonyForcePhysicsState()
    TonyCameraProbe()
    TonyCameraForceMode()
    TonyCameraViewportProbe()
    TonyCameraTimingProbe()
    TonyCameraPointSelectProbe()
    TonyCameraPointStateProbe()
    TonyCameraEffectsProbe()
    TonyViewProjectionProbe()
    TonyMovementPhysicsProbe()
    TonyGroundMotionProbe()
    TonyGroundMotionWriters()
    TonyOllieRandomProbe()
    TonySharedRandomProbe()
    TonyVelocityDampingRandomProbe()
    TonyVelocityDampingComponentProbe()
    TonyAirCorrectionProbe()
    TonyResponseCorrectionProbe()
    TonyMotionCorrectionProbe()
    TonyGroundMotionProfileProbe()
    TonyInAirProbe()
    TonySpecialPhysicsProbe()
    TonyAirCollisionProbe()
    TonyPhysicsStateRequestProbe()
    TonyPhysicsStateWriterProbe()
    TonyOllieLatchProbe()
    TonyViewProjectionPerturb()
    TonyCameraPositionProbe()
    TonyCameraCollisionProbe()
    TonyActorSubmissionProbe()
    TonyGeometrySubmissionProbe()
    TonyTransformedVertexProbe()
    TonyPlayerDiff()
    TonyPositionCommitProbe()
    TonyCollisionProbe()
    TonyCollisionInitBoundaryProbe()
    TonyCollisionLoaderProbe()
    TonyCollisionModelKindProbe()
    TonyCollisionFlagsProbe()
    TonyCollisionDynamicProbe()
    TonyCollisionDynamicCullProbe()
    TonyCollisionDynamicTransformProbe()
    TonyCollisionDynamicTransformMutationProbe()
    TonyType192CommandProbe()
    install_recording_instrumentation()
    _registered = True
    write(
        "OpenTony GDB helpers loaded: tony-read8, tony-read16, tony-read32, tony-readf, "
        "tony-hexdump, tony-dump, tony-snapshot, tony-diff, tony-modules, tony-bp, "
        "tony-thps2, tony-bp-thps2, "
        "tony-skip-movies, tony-force-level, tony-player-sample, tony-input-sample, "
        "tony-record-start, tony-record-stop, tony-record-toggle, tony-record-status, "
        "tony-record-forensic, "
        "tony-replay-retail, "
        "tony-action-edge, tony-jump-edge, "
        "tony-key-loop, tony-key-clear, tony-animation-sample, tony-animation-request-sample, "
        "tony-animation-selector-sample, "
        "tony-watch, tony-watch-once, tony-watch-batch, tony-watch-log, tony-watch-clear, "
        "tony-trace-open, tony-trace-close, tony-frame-clock, tony-physics-probe, "
        "tony-simulation-time-probe, "
        "tony-simulation-time-accumulator-probe, "
        "tony-timer-callback-probe, "
        "tony-force-physics-state, "
        "tony-camera-probe, tony-camera-effects-probe, tony-view-probe, tony-view-perturb, "
        "tony-camera-position-probe, tony-actor-probe, tony-geometry-probe, "
        "tony-player-diff, tony-position-commit, "
        "tony-collision-probe, "
        "tony-collision-init-boundaries, "
        "tony-movement-physics-probe, "
        "tony-ground-motion-probe, tony-ground-motion-writers, "
        "tony-ollie-random-probe, "
        "tony-rng-probe, "
        "tony-velocity-damping-random-probe, "
        "tony-velocity-damping-component-probe, "
        "tony-air-correction-probe, "
        "tony-response-correction-probe, "
        "tony-motion-correction-probe, "
        "tony-ground-motion-profile-probe, "
        "tony-in-air-probe, tony-air-collision-probe, tony-physics-state-requests, "
        "tony-special-physics-probe, "
        "tony-physics-state-writers, "
        "tony-ollie-latch-probe, "
        "tony-player-diff, tony-position-commit, "
        "tony-collision-loader-probe, tony-collision-model-kind-probe, "
        "tony-collision-flags-probe, "
        "tony-collision-dynamic-probe, tony-collision-dynamic-cull-probe, "
        "tony-collision-transform-probe, tony-collision-transform-mutate, "
        "tony-trg-type192-probe, "
        "tony-skip-movies, tony-force-level, tony-frontend-play, tony-frontend-confirm, "
        "tony-player-sample, tony-input-sample, "
        "tony-action-sequence, "
        "tony-watch, tony-watch-once, tony-watch-batch, tony-watch-log, tony-watch-clear, "
        "tony-trace-open, tony-trace-close, tony-frame-clock, tony-physics-probe, "
        "tony-camera-probe, tony-camera-force-mode, tony-camera-timing-probe, "
        "tony-camera-viewport-probe, "
        "tony-camera-point-probe, "
        "tony-camera-point-state-probe, "
        "tony-camera-effects-probe, "
        "tony-view-probe, tony-view-perturb, "
        "tony-camera-position-probe, tony-camera-collision-probe, "
        "tony-actor-probe, tony-geometry-probe, tony-transformed-vertices, "
        "tony-player-diff, tony-position-commit"
    )
