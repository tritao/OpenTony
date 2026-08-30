#ifndef OPENTONY_CAPTURE_SHARED_BUFFER_H
#define OPENTONY_CAPTURE_SHARED_BUFFER_H

#include "capture_layout.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CaptureBuffer {
    unsigned char *mapping;
    uint32_t mapping_size;
} CaptureBuffer;

int ot_capture_buffer_init(CaptureBuffer *buffer, void *mapping, uint32_t mapping_size);
void ot_capture_buffer_fail(CaptureBuffer *buffer, uint32_t status, uint32_t error_code);
uint32_t ot_capture_action_mask(const CaptureConfig *config, uint32_t frame_index);
int ot_capture_append_frame(
    CaptureBuffer *buffer,
    uint32_t frame_index,
    uint32_t input_mask,
    uint32_t input_flags,
    uint32_t player_address,
    const uint8_t *before,
    const uint8_t *after,
    const CaptureTimingSnapshot *timing_before,
    const CaptureTimingSnapshot *timing_after,
    const CaptureCausalEvent *causal_events,
    uint32_t causal_event_count,
    const CaptureTimerSample *timer_samples,
    uint32_t timer_sample_count);

#ifdef __cplusplus
}
#endif

#endif /* OPENTONY_CAPTURE_SHARED_BUFFER_H */
