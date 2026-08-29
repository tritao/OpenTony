#ifndef OPENTONY_CAPTURE_HOOKS_H
#define OPENTONY_CAPTURE_HOOKS_H

#include <stdint.h>

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

/* M1 is observation-only.  This entry point is deliberately a no-op until
 * the physics detour has passed same-run GDB equivalence (M2/M3). */
int ot_capture_install_hooks(void *module_base);

#ifdef __cplusplus
}
#endif

#endif /* OPENTONY_CAPTURE_HOOKS_H */
