#ifndef OPENTONY_CAPTURE_HOOKS_H
#define OPENTONY_CAPTURE_HOOKS_H

#include "shared_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CaptureHookSpec {
    const char *name;
    uint32_t rva;
    uint8_t expected[16];
    uint32_t expected_size;
    uint32_t overwrite_size;
} CaptureHookSpec;

const CaptureHookSpec *ot_capture_hook_manifest(uint32_t *count);
int ot_capture_verify_build(void *module_base, const uint8_t expected_sha256[32]);
int ot_capture_verify_hooks(void *module_base, uint32_t *failed_index);

/* Bind the process-local mapping before installing the first detour. */
int ot_capture_bind_buffer(CaptureBuffer *buffer);

/* M2 installs only the proven physics-frame detour.  Input and timer seams
 * remain manifest-only until their same-run equivalence milestones. */
int ot_capture_install_hooks(void *module_base);

/* These helpers are called by the x86 naked wrapper around Skater_PhysicsFrame.
 * Keep their ABI C-compatible: the wrapper must preserve the game's register
 * and return-value contract exactly. */
void __cdecl ot_capture_physics_before(uint32_t player);
void __cdecl ot_capture_physics_after(void);

#ifdef __cplusplus
}
#endif

#endif /* OPENTONY_CAPTURE_HOOKS_H */
