#include "hook_engine.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string.h>

int ot_capture_create_trampoline(
    unsigned char *target,
    const CaptureHookSpec *spec,
    uint8_t *original,
    void **trampoline_out) {
    unsigned char *trampoline;
    long relative;
    long absolute;
    uint32_t size;
    uint32_t index;
    uint32_t relocation_offset;

    if (target == 0 || spec == 0 || spec->overwrite_size < 5u ||
        original == 0 || trampoline_out == 0 ||
        spec->overwrite_size > 16u) {
        return 0;
    }
    size = spec->overwrite_size + 5u;
    trampoline = (unsigned char *)VirtualAlloc(
        0, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (trampoline == 0) {
        return 0;
    }
    memcpy(original, target, spec->overwrite_size);
    memcpy(trampoline, target, spec->overwrite_size);
    /* Relocate only the rel32 instructions declared by the verified hook
     * manifest.  This deliberately avoids pseudo-disassembling arbitrary
     * bytes: a missing or mismatched opcode fails closed. */
    if (spec->rel32_count > OTCAP_MAX_REL32_RELOCATIONS) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return 0;
    }
    for (index = 0; index < spec->rel32_count; ++index) {
        relocation_offset = spec->rel32_offsets[index];
        if (relocation_offset + 5u > spec->overwrite_size ||
            (trampoline[relocation_offset] != 0xe8 &&
             trampoline[relocation_offset] != 0xe9)) {
            VirtualFree(trampoline, 0, MEM_RELEASE);
            return 0;
        }
        memcpy(&relative, trampoline + relocation_offset + 1u, sizeof(relative));
        absolute = (long)(target + relocation_offset + 5u) + relative;
        relative = absolute - (long)(trampoline + relocation_offset + 5u);
        memcpy(trampoline + relocation_offset + 1u, &relative, sizeof(relative));
    }
    trampoline[spec->overwrite_size] = 0xe9;
    relative = (long)(target + spec->overwrite_size -
        (trampoline + spec->overwrite_size + 5u));
    memcpy(trampoline + spec->overwrite_size + 1u, &relative, sizeof(relative));
    *trampoline_out = trampoline;
    return 1;
}

int ot_capture_patch_target(
    unsigned char *target,
    uint32_t overwrite_size,
    void *hook,
    const uint8_t *original) {
    DWORD old_protect;
    DWORD restored_protect;
    long relative;
    uint32_t index;

#if !defined(_MSC_VER) || !defined(_M_IX86)
    (void)target;
    (void)overwrite_size;
    (void)hook;
    (void)original;
    return 0;
#else
    if (target == 0 || overwrite_size < 5u || hook == 0 || original == 0) {
        return 0;
    }
    if (!VirtualProtect(
            target, overwrite_size, PAGE_EXECUTE_READWRITE, &old_protect)) {
        return 0;
    }
    target[0] = 0xe9;
    relative = (long)((unsigned char *)hook - (target + 5u));
    memcpy(target + 1u, &relative, sizeof(relative));
    for (index = 5u; index < overwrite_size; ++index) {
        target[index] = 0x90;
    }
    FlushInstructionCache(GetCurrentProcess(), target, overwrite_size);
    if (VirtualProtect(target, overwrite_size, old_protect, &restored_protect)) {
        return 1;
    }
    /* A protection-restore failure is fail-closed: put the exact original
     * bytes back before reporting installation failure. */
    if (VirtualProtect(
            target, overwrite_size, PAGE_EXECUTE_READWRITE, &restored_protect)) {
        memcpy(target, original, overwrite_size);
        FlushInstructionCache(GetCurrentProcess(), target, overwrite_size);
        VirtualProtect(target, overwrite_size, old_protect, &restored_protect);
    }
    return 0;
#endif
}

int ot_capture_restore_target(
    unsigned char *target,
    uint32_t overwrite_size,
    const uint8_t *original) {
    DWORD old_protect;
    DWORD restored_protect;

#if !defined(_MSC_VER) || !defined(_M_IX86)
    (void)target;
    (void)overwrite_size;
    (void)original;
    return 0;
#else
    if (target == 0 || original == 0 || overwrite_size == 0) {
        return 0;
    }
    if (!VirtualProtect(
            target, overwrite_size, PAGE_EXECUTE_READWRITE, &old_protect)) {
        return 0;
    }
    memcpy(target, original, overwrite_size);
    FlushInstructionCache(GetCurrentProcess(), target, overwrite_size);
    return VirtualProtect(target, overwrite_size, old_protect, &restored_protect) != 0;
#endif
}
