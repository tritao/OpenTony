#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "capture_layout.h"
#include "hooks.h"
#include "shared_buffer.h"

#include <stdio.h>
#include <string.h>

static HANDLE g_mapping_handle = 0;
static CaptureBuffer g_buffer = {0, 0};

static void capture_fail(uint32_t error_code) {
    ot_capture_buffer_fail(&g_buffer, OTCAP_STATUS_FAILED, error_code);
}

static DWORD WINAPI capture_initialize(void *unused) {
    char mapping_name[256];
    DWORD length;
    CaptureHeader *header;
    HMODULE module;
    uint32_t failed_hook = 0;
    (void)unused;
    ZeroMemory(mapping_name, sizeof(mapping_name));
    length = GetEnvironmentVariableA(OTCAP_MAPPING_ENV, mapping_name, sizeof(mapping_name));
    if (length == 0 || length >= sizeof(mapping_name)) {
        OutputDebugStringA("OpenTony capture: mapping name is missing\n");
        return 0;
    }
    g_mapping_handle = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, mapping_name);
    if (g_mapping_handle == 0) {
        OutputDebugStringA("OpenTony capture: OpenFileMappingA failed\n");
        return 0;
    }
    if (!ot_capture_buffer_init(&g_buffer, MapViewOfFile(g_mapping_handle, FILE_MAP_ALL_ACCESS, 0, 0, 0), OTCAP_MAPPING_SIZE)) {
        return 0;
    }
    header = (CaptureHeader *)g_buffer.mapping;
    module = GetModuleHandleA(0);
    if (!ot_capture_verify_build(module, header->build_sha256)) {
        capture_fail(OTCAP_ERROR_BUILD_IDENTITY);
        return 0;
    }
    if (!ot_capture_verify_hooks(module, &failed_hook)) {
        (void)failed_hook;
        capture_fail(OTCAP_ERROR_HOOK_BYTES);
        return 0;
    }
    /* M1 validates the seams but installs no detours. */
    if (!ot_capture_install_hooks(module)) {
        capture_fail(OTCAP_ERROR_HOOK_BYTES);
        return 0;
    }
    InterlockedExchange((LONG *)&header->status, OTCAP_STATUS_READY);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    HANDLE thread;
    (void)instance;
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        thread = CreateThread(0, 0, capture_initialize, 0, 0, 0);
        if (thread != 0) {
            CloseHandle(thread);
        }
    }
    return TRUE;
}

/* Exported for the M2 detour and for focused host-side smoke tests. */
__declspec(dllexport) uint32_t opentony_capture_action_mask(uint32_t frame_index) {
    CaptureHeader *header;
    if (g_buffer.mapping == 0) {
        return 0;
    }
    header = (CaptureHeader *)g_buffer.mapping;
    return ot_capture_action_mask((CaptureConfig *)(g_buffer.mapping + header->config_offset), frame_index);
}
