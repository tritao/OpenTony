#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "shared_buffer.h"

#include <string.h>

int ot_capture_buffer_init(CaptureBuffer *buffer, void *mapping, uint32_t mapping_size) {
    CaptureHeader *header;
    CaptureConfig *config;
    if (buffer == 0 || mapping == 0 || mapping_size < OTCAP_HEADER_BYTES + OTCAP_CONFIG_BYTES) {
        return 0;
    }
    buffer->mapping = (unsigned char *)mapping;
    buffer->mapping_size = mapping_size;
    header = (CaptureHeader *)buffer->mapping;
    if (memcmp(header->magic, OTCAP_MAGIC, OTCAP_MAGIC_SIZE) != 0 ||
        header->version != OTCAP_VERSION ||
        header->header_size != OTCAP_HEADER_BYTES ||
        header->mapping_size != mapping_size ||
        header->config_offset > mapping_size || header->config_size > mapping_size - header->config_offset ||
        header->initial_state_offset > mapping_size ||
        header->initial_state_size > mapping_size - header->initial_state_offset ||
        header->initial_state_size != OTCAP_INITIAL_STATE_BYTES ||
        header->data_offset > mapping_size ||
        header->frame_limit == 0 || header->frame_limit > otcap_max_frames(mapping_size, header->data_offset) ||
        header->config_size != OTCAP_CONFIG_BYTES) {
        ot_capture_buffer_fail(buffer, OTCAP_STATUS_FAILED, OTCAP_ERROR_INVALID_CONFIG);
        return 0;
    }
    if (header->data_offset < otcap_align4096(header->initial_state_offset + header->initial_state_size) ||
        header->bytes_used < header->data_offset) {
        ot_capture_buffer_fail(buffer, OTCAP_STATUS_FAILED, OTCAP_ERROR_INVALID_CONFIG);
        return 0;
    }
    config = (CaptureConfig *)(buffer->mapping + header->config_offset);
    if (config->version != OTCAP_VERSION || config->size != OTCAP_CONFIG_BYTES ||
        config->frame_limit != header->frame_limit || config->action_count > OTCAP_MAX_ACTION_INTERVALS) {
        ot_capture_buffer_fail(buffer, OTCAP_STATUS_FAILED, OTCAP_ERROR_INVALID_CONFIG);
        return 0;
    }
    return 1;
}

void ot_capture_buffer_fail(CaptureBuffer *buffer, uint32_t status, uint32_t error_code) {
    CaptureHeader *header;
    if (buffer == 0 || buffer->mapping == 0) {
        return;
    }
    header = (CaptureHeader *)buffer->mapping;
    InterlockedExchange((LONG *)&header->error_code, (LONG)error_code);
    InterlockedExchange((LONG *)&header->status, (LONG)status);
}

uint32_t ot_capture_action_mask(const CaptureConfig *config, uint32_t frame_index) {
    uint32_t mask = 0;
    uint32_t index;
    if (config == 0) {
        return 0;
    }
    for (index = 0; index < config->action_count; ++index) {
        const CaptureActionInterval *action = &config->actions[index];
        if (frame_index >= action->start_frame &&
            frame_index - action->start_frame < action->hold_frames) {
            mask |= action->action_mask;
        }
    }
    return mask;
}

int ot_capture_append_frame(
    CaptureBuffer *buffer,
    uint32_t frame_index,
    uint32_t input_mask,
    uint32_t input_flags,
    uint32_t player_address,
    const uint8_t *before,
    const uint8_t *after,
    const CaptureTimingSnapshot *timing_before,
    const CaptureTimingSnapshot *timing_after) {
    CaptureHeader *header;
    CaptureFrameRecord *record;
    uint32_t slot;
    uint32_t max_frames;
    if (buffer == 0 || buffer->mapping == 0 || before == 0 || after == 0) {
        return 0;
    }
    header = (CaptureHeader *)buffer->mapping;
    max_frames = otcap_max_frames(buffer->mapping_size, header->data_offset);
    slot = (uint32_t)InterlockedIncrement((LONG *)&header->frame_count) - 1u;
    if (slot >= header->frame_limit || slot >= max_frames || frame_index != slot) {
        ot_capture_buffer_fail(buffer, OTCAP_STATUS_OVERFLOW, OTCAP_ERROR_OVERFLOW);
        return 0;
    }
    record = (CaptureFrameRecord *)(buffer->mapping + header->data_offset) + slot;
    ZeroMemory(record, sizeof(*record));
    record->header.frame_index = frame_index;
    record->header.input_mask = input_mask;
    record->header.input_flags = input_flags;
    record->header.player_address = player_address;
    record->header.before_size = OTCAP_PLAYER_BLOB_SIZE;
    record->header.after_size = OTCAP_PLAYER_BLOB_SIZE;
    memcpy(record->player_before, before, OTCAP_PLAYER_BLOB_SIZE);
    memcpy(record->player_after, after, OTCAP_PLAYER_BLOB_SIZE);
    if (timing_before != 0) {
        memcpy(&record->timing_before, timing_before, sizeof(record->timing_before));
    }
    if (timing_after != 0) {
        memcpy(&record->timing_after, timing_after, sizeof(record->timing_after));
    }
    InterlockedExchange((LONG *)&header->bytes_used,
        (LONG)(header->data_offset + (slot + 1u) * OTCAP_FRAME_BYTES));
    InterlockedExchange((LONG *)&header->status, OTCAP_STATUS_CAPTURING);
    return 1;
}
