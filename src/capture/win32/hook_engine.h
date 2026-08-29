#ifndef OPENTONY_CAPTURE_HOOK_ENGINE_H
#define OPENTONY_CAPTURE_HOOK_ENGINE_H

#include "hooks.h"

#ifdef __cplusplus
extern "C" {
#endif

int ot_capture_create_trampoline(
    unsigned char *target,
    const CaptureHookSpec *spec,
    uint8_t *original,
    void **trampoline_out);
int ot_capture_patch_target(
    unsigned char *target,
    uint32_t overwrite_size,
    void *hook,
    const uint8_t *original);
int ot_capture_restore_target(
    unsigned char *target,
    uint32_t overwrite_size,
    const uint8_t *original);

#ifdef __cplusplus
}
#endif

#endif /* OPENTONY_CAPTURE_HOOK_ENGINE_H */
