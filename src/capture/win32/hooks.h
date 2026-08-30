#ifndef OPENTONY_CAPTURE_HOOKS_H
#define OPENTONY_CAPTURE_HOOKS_H

#include "shared_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OTCAP_MAX_REL32_RELOCATIONS 4u

typedef struct CaptureHookSpec {
    const char *name;
    uint32_t rva;
    uint8_t expected[16];
    uint32_t expected_size;
    uint32_t overwrite_size;
    /* Offsets of the verified rel32 instructions copied into the trampoline.
     * The recorder never guesses instruction boundaries in retail code. */
    uint8_t rel32_offsets[OTCAP_MAX_REL32_RELOCATIONS];
    uint32_t rel32_count;
} CaptureHookSpec;

const CaptureHookSpec *ot_capture_hook_manifest(uint32_t *count);
int ot_capture_verify_build(void *module_base, const uint8_t expected_sha256[32]);
int ot_capture_verify_hooks(void *module_base, uint32_t *failed_index);

/* Bind the process-local mapping before installing the first detour. */
int ot_capture_bind_buffer(CaptureBuffer *buffer);

/* Install the proven gameplay, timer, and deterministic frontend detours. */
int ot_capture_install_hooks(void *module_base);

/* These helpers are called by x86 naked wrappers. Keep their ABI C-compatible:
 * the wrappers must preserve the game's register and return-value contract. */
void __cdecl ot_capture_physics_before(uint32_t player);
void __cdecl ot_capture_physics_after(void);
void __cdecl ot_capture_simulation_time_store(uint32_t player, uint32_t value);
void __cdecl ot_capture_shared_random_call(
    uint32_t caller, uint32_t argument, uint32_t result);
void __cdecl ot_capture_input_boundary(void);
void __cdecl ot_capture_timer_boundary(uint32_t phase);
void __cdecl ot_capture_frontend_play_boundary(uint32_t stack_pointer);
void __cdecl ot_capture_frontend_level_boundary(void);
void __cdecl ot_capture_launch_level_boundary(uint32_t stack_pointer);
void __cdecl ot_capture_frontend_key_boundary(uint32_t stack_pointer);
void __cdecl ot_capture_frontend_summary_boundary(void);

#ifdef __cplusplus
}
#endif

#endif /* OPENTONY_CAPTURE_HOOKS_H */
