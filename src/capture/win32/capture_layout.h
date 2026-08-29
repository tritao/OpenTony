#ifndef OPENTONY_CAPTURE_LAYOUT_H
#define OPENTONY_CAPTURE_LAYOUT_H

/*
 * Shared wire layout for the Windows recorder.
 *
 * This header intentionally contains only fixed-width C types.  It is shared
 * by the VC6-compatible injected DLL, the capture host, and the offline
 * decoder; no C++ runtime or STL type may cross this boundary.
 */

#include <stddef.h>

/* VC6 predates the C99 stdint header. */
#if defined(_MSC_VER) && _MSC_VER < 1600
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif

#define OTCAP_MAGIC_SIZE 8u
#define OTCAP_VERSION 1u
#define OTCAP_MAPPING_SIZE (64u * 1024u * 1024u)
#define OTCAP_MAX_ACTION_INTERVALS 128u
#define OTCAP_MAX_TIMER_SAMPLES 8u
#define OTCAP_MAX_CAUSAL_EVENTS 16u
#define OTCAP_PLAYER_BLOB_SIZE 0x3210u
#define OTCAP_BUILD_SHA256_SIZE 32u
#define OTCAP_MAPPING_ENV "OPENTONY_CAPTURE_MAPPING"

/* The bytes spell OTCAP followed by a NUL, version, and two reserved bytes. */
static const uint8_t OTCAP_MAGIC[OTCAP_MAGIC_SIZE] = {
    'O', 'T', 'C', 'A', 'P', 0, 0, OTCAP_VERSION
};

enum {
    OTCAP_STATUS_INITIALIZING = 0,
    OTCAP_STATUS_READY = 1,
    OTCAP_STATUS_CAPTURING = 2,
    OTCAP_STATUS_COMPLETE = 3,
    OTCAP_STATUS_FAILED = 4,
    OTCAP_STATUS_OVERFLOW = 5
};

enum {
    OTCAP_ERROR_NONE = 0,
    OTCAP_ERROR_BAD_MAPPING = 1,
    OTCAP_ERROR_BUILD_IDENTITY = 2,
    OTCAP_ERROR_HOOK_BYTES = 3,
    OTCAP_ERROR_INVALID_CONFIG = 4,
    OTCAP_ERROR_OVERFLOW = 5,
    OTCAP_ERROR_SNAPSHOT = 6
};

enum {
    OTCAP_TIMER_PHASE_PHYSICS_ENTRY = 1,
    OTCAP_TIMER_PHASE_TIMER_UPDATE = 2,
    OTCAP_TIMER_PHASE_CLOCK_READ = 3,
    OTCAP_TIMER_PHASE_POST_PHYSICS = 4
};

enum {
    OTCAP_INPUT_FLAG_AIR_CONTROL = 1,
    OTCAP_INPUT_FLAG_INJECTED = 2
};

#pragma pack(push, 1)

typedef struct CaptureActionInterval {
    uint32_t action_mask;
    uint32_t start_frame;
    uint32_t hold_frames;
} CaptureActionInterval;

typedef struct CaptureHeader {
    uint8_t magic[OTCAP_MAGIC_SIZE];
    uint32_t version;
    uint32_t header_size;
    uint32_t config_offset;
    uint32_t config_size;
    uint32_t initial_state_offset;
    uint32_t initial_state_size;
    uint32_t data_offset;
    uint32_t mapping_size;
    volatile uint32_t bytes_used;
    volatile uint32_t frame_count;
    uint32_t frame_limit;
    volatile uint32_t status;
    volatile uint32_t error_code;
    uint32_t level_index;
    uint32_t image_base;
    uint32_t player_blob_size;
    uint32_t process_id;
    uint8_t build_sha256[OTCAP_BUILD_SHA256_SIZE];
} CaptureHeader;

typedef struct CaptureConfig {
    uint32_t version;
    uint32_t size;
    uint32_t frame_limit;
    uint32_t action_count;
    uint32_t level_index;
    uint32_t flags;
    uint8_t build_sha256[OTCAP_BUILD_SHA256_SIZE];
    CaptureActionInterval actions[OTCAP_MAX_ACTION_INTERVALS];
} CaptureConfig;

/* Initial timer state is copied verbatim and decoded offline. */
typedef struct CaptureInitialState {
    uint32_t size;
    uint32_t timer_handle;
    uint32_t interval_ms;
    uint32_t opaque_08;
    uint32_t accumulated_ms;
    uint32_t opaque_10;
    uint64_t public_accumulator_raw;
    uint64_t simulation_accumulator_raw;
    uint32_t public_tick;
    uint32_t simulation_time;
    uint32_t simulation_pause_gate_a;
    uint32_t simulation_pause_gate_b;
} CaptureInitialState;

typedef struct CaptureTimingSnapshot {
    uint32_t animation_clock;
    uint32_t animation_time_scale;
    uint32_t animation_time_scale_square;
    uint32_t animation_clock_accumulator;
    uint32_t simulation_time;
    uint32_t timing_delta_q11;
} CaptureTimingSnapshot;

/* Raw timer boundary observations; doubles remain raw IEEE-754 words. */
typedef struct CaptureTimerSample {
    uint32_t phase;
    uint32_t frame_index;
    uint32_t interval_ms;
    uint32_t accumulated_ms;
    uint32_t public_tick;
    uint32_t simulation_time;
    uint32_t pause_gate_a;
    uint32_t pause_gate_b;
    uint64_t public_accumulator_raw;
    uint64_t simulation_accumulator_raw;
} CaptureTimerSample;

typedef struct CaptureCausalEvent {
    uint32_t type;
    uint32_t phase;
    uint32_t frame_index;
    uint32_t size;
    uint8_t payload[64];
} CaptureCausalEvent;

typedef struct CaptureFrameHeader {
    uint32_t frame_index;
    uint32_t input_mask;
    uint32_t input_flags;
    uint32_t player_address;
    uint32_t timer_sample_count;
    uint32_t causal_event_count;
    uint32_t before_size;
    uint32_t after_size;
    uint32_t flags;
} CaptureFrameHeader;

/* A bounded scenario uses one fixed record per frame: no dropped records. */
typedef struct CaptureFrameRecord {
    CaptureFrameHeader header;
    uint8_t player_before[OTCAP_PLAYER_BLOB_SIZE];
    uint8_t player_after[OTCAP_PLAYER_BLOB_SIZE];
    CaptureTimingSnapshot timing_before;
    CaptureTimingSnapshot timing_after;
    CaptureTimerSample timer_samples[OTCAP_MAX_TIMER_SAMPLES];
    CaptureCausalEvent causal_events[OTCAP_MAX_CAUSAL_EVENTS];
} CaptureFrameRecord;

#pragma pack(pop)

#define OTCAP_HEADER_BYTES ((uint32_t)sizeof(CaptureHeader))
#define OTCAP_CONFIG_BYTES ((uint32_t)sizeof(CaptureConfig))
#define OTCAP_INITIAL_STATE_BYTES ((uint32_t)sizeof(CaptureInitialState))
#define OTCAP_FRAME_BYTES ((uint32_t)sizeof(CaptureFrameRecord))

#ifdef __cplusplus
extern "C" {
#endif

static uint32_t otcap_align4096(uint32_t value) {
    return (value + 4095u) & ~4095u;
}

static uint32_t otcap_max_frames(uint32_t mapping_size, uint32_t data_offset) {
    if (mapping_size <= data_offset) {
        return 0;
    }
    return (mapping_size - data_offset) / OTCAP_FRAME_BYTES;
}

#ifdef __cplusplus
}
#endif

#endif /* OPENTONY_CAPTURE_LAYOUT_H */
