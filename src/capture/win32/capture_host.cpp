#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "capture_layout.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void) {
    fprintf(stderr,
        "usage: opentony_capture_host.exe --exe FILE --dll FILE --output FILE "
        "--build-sha256 HEX --frames N [--level N] [--force] [--action MASK:START:HOLD]...\n");
}

static int parse_u32(const char *text, uint32_t *value) {
    char *end = 0;
    unsigned long parsed;
    if (text == 0 || *text == 0 || value == 0) {
        return 0;
    }
    parsed = strtoul(text, &end, 0);
    if (*end != 0 || parsed > 0xffffffffUL) {
        return 0;
    }
    *value = (uint32_t)parsed;
    return 1;
}

static int parse_sha256(const char *text, uint8_t output[32]) {
    uint32_t index;
    if (text == 0 || strlen(text) != 64) {
        return 0;
    }
    for (index = 0; index < 32; ++index) {
        unsigned int value;
        if (sscanf(text + index * 2, "%2x", &value) != 1 || value > 0xff) {
            return 0;
        }
        output[index] = (uint8_t)value;
    }
    return 1;
}

static int parse_action(const char *text, CaptureActionInterval *action) {
    unsigned long mask;
    unsigned long start;
    unsigned long hold;
    char tail;
    if (text == 0 || action == 0 || sscanf(text, "%lx:%lu:%lu%c", &mask, &start, &hold, &tail) != 3 ||
        mask > 0xffffffffUL || start > 0xffffffffUL || hold == 0 || hold > 0xffffffffUL) {
        return 0;
    }
    action->action_mask = (uint32_t)mask;
    action->start_frame = (uint32_t)start;
    action->hold_frames = (uint32_t)hold;
    return 1;
}

static int append_command_argument(char *command, size_t capacity, const char *value) {
    size_t used = strlen(command);
    size_t length = strlen(value);
    if (used + length + 4 >= capacity) {
        return 0;
    }
    command[used++] = ' ';
    command[used++] = '"';
    memcpy(command + used, value, length);
    used += length;
    command[used++] = '"';
    command[used] = 0;
    return 1;
}

static int inject_dll(HANDLE process, const char *dll_path) {
    SIZE_T bytes;
    LPVOID remote_path;
    HANDLE thread;
    HMODULE kernel;
    FARPROC loader;
    DWORD result = 0;
    bytes = strlen(dll_path) + 1;
    remote_path = VirtualAllocEx(process, 0, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (remote_path == 0 || !WriteProcessMemory(process, remote_path, dll_path, bytes, &bytes)) {
        if (remote_path != 0) VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
        return 0;
    }
    kernel = GetModuleHandleA("kernel32.dll");
    loader = kernel == 0 ? 0 : GetProcAddress(kernel, "LoadLibraryA");
    thread = loader == 0 ? 0 : CreateRemoteThread(process, 0, 0,
        (LPTHREAD_START_ROUTINE)loader, remote_path, 0, 0);
    if (thread == 0) {
        VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
        return 0;
    }
    if (WaitForSingleObject(thread, 10000) != WAIT_OBJECT_0) {
        CloseHandle(thread);
        VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
        return 0;
    }
    GetExitCodeThread(thread, &result);
    CloseHandle(thread);
    VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
    return result != 0;
}

static int wait_for_ready(CaptureHeader *header, HANDLE process) {
    uint32_t elapsed;
    for (elapsed = 0; elapsed < 10000u; ++elapsed) {
        if (header->status == OTCAP_STATUS_READY) {
            return 1;
        }
        if (header->status == OTCAP_STATUS_FAILED || header->status == OTCAP_STATUS_OVERFLOW ||
            WaitForSingleObject(process, 0) == WAIT_OBJECT_0) {
            return 0;
        }
        Sleep(1);
    }
    return 0;
}

static DWORD wait_for_bounded_capture(CaptureHeader *header, HANDLE process) {
    for (;;) {
        DWORD result = WaitForSingleObject(process, 50);
        if (result == WAIT_OBJECT_0 || result == WAIT_FAILED) {
            return result;
        }
        if (header->status == OTCAP_STATUS_FAILED ||
            header->status == OTCAP_STATUS_OVERFLOW) {
            TerminateProcess(process, 1);
            return WaitForSingleObject(process, INFINITE);
        }
        if (header->status == OTCAP_STATUS_COMPLETE ||
            header->frame_count >= header->frame_limit) {
            /* The final record is published before frame_count advances.  A
             * bounded capture therefore has no reason to leave the retail
             * frontend running (and cannot depend on a debugger-issued quit). */
            InterlockedExchange((LONG *)&header->status, OTCAP_STATUS_COMPLETE);
            TerminateProcess(process, 0);
            return WaitForSingleObject(process, INFINITE);
        }
    }
}

int main(int argc, char **argv) {
    const char *exe_path = 0;
    const char *dll_path = 0;
    const char *output_path = 0;
    const char *sha_text = 0;
    char mapping_name[128];
    char command_line[2048];
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    HANDLE mapping;
    void *view;
    CaptureHeader *header;
    CaptureConfig *config;
    CaptureInitialState *initial;
    uint32_t frames = 0;
    uint32_t level = 0;
    uint32_t action_count = 0;
    int force = 0;
    CaptureActionInterval actions[OTCAP_MAX_ACTION_INTERVALS];
    uint32_t index;
    DWORD wait_result;
    HANDLE output;
    DWORD written;
    int exit_code = 1;

    ZeroMemory(&startup, sizeof(startup));
    startup.cb = sizeof(startup);
    ZeroMemory(&process, sizeof(process));
    ZeroMemory(actions, sizeof(actions));
    for (index = 1; index < (uint32_t)argc; ++index) {
        if (strcmp(argv[index], "--exe") == 0 && index + 1 < (uint32_t)argc) exe_path = argv[++index];
        else if (strcmp(argv[index], "--dll") == 0 && index + 1 < (uint32_t)argc) dll_path = argv[++index];
        else if (strcmp(argv[index], "--output") == 0 && index + 1 < (uint32_t)argc) output_path = argv[++index];
        else if (strcmp(argv[index], "--build-sha256") == 0 && index + 1 < (uint32_t)argc) sha_text = argv[++index];
        else if (strcmp(argv[index], "--frames") == 0 && index + 1 < (uint32_t)argc) { if (!parse_u32(argv[++index], &frames)) goto done; }
        else if (strcmp(argv[index], "--level") == 0 && index + 1 < (uint32_t)argc) { if (!parse_u32(argv[++index], &level)) goto done; }
        else if (strcmp(argv[index], "--force") == 0) force = 1;
        else if (strcmp(argv[index], "--action") == 0 && index + 1 < (uint32_t)argc) {
            if (action_count >= OTCAP_MAX_ACTION_INTERVALS) goto done;
            if (!parse_action(argv[++index], &actions[action_count])) goto done;
            ++action_count;
        } else { usage(); goto done; }
    }
    if (exe_path == 0 || dll_path == 0 || output_path == 0 || sha_text == 0 || frames == 0 || frames > 4096) {
        usage();
        goto done;
    }
    if (!force && GetFileAttributesA(output_path) != INVALID_FILE_ATTRIBUTES) {
        fprintf(stderr, "refusing to overwrite %s; pass --force\n", output_path);
        goto done;
    }
    mapping_name[0] = 0;
    _snprintf(mapping_name, sizeof(mapping_name) - 1, "Local\\OpenTonyCapture-%lu", (unsigned long)GetCurrentProcessId());
    mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, 0, OTCAP_MAPPING_SIZE, mapping_name);
    if (mapping == 0) goto done;
    view = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, OTCAP_MAPPING_SIZE);
    if (view == 0) { CloseHandle(mapping); goto done; }
    ZeroMemory(view, OTCAP_MAPPING_SIZE);
    header = (CaptureHeader *)view;
    config = (CaptureConfig *)((unsigned char *)view + OTCAP_HEADER_BYTES);
    initial = (CaptureInitialState *)((unsigned char *)config + OTCAP_CONFIG_BYTES);
    header->version = OTCAP_VERSION;
    memcpy(header->magic, OTCAP_MAGIC, OTCAP_MAGIC_SIZE);
    header->header_size = OTCAP_HEADER_BYTES;
    header->config_offset = OTCAP_HEADER_BYTES;
    header->config_size = OTCAP_CONFIG_BYTES;
    header->initial_state_offset = OTCAP_HEADER_BYTES + OTCAP_CONFIG_BYTES;
    header->initial_state_size = OTCAP_INITIAL_STATE_BYTES;
    header->data_offset = otcap_align4096(header->initial_state_offset + header->initial_state_size);
    header->mapping_size = OTCAP_MAPPING_SIZE;
    header->bytes_used = header->data_offset;
    header->frame_limit = frames;
    header->level_index = level;
    header->image_base = 0x00400000u;
    header->player_blob_size = OTCAP_PLAYER_BLOB_SIZE;
    header->process_id = GetCurrentProcessId();
    if (!parse_sha256(sha_text, header->build_sha256)) goto cleanup_view;
    config->version = OTCAP_VERSION;
    config->size = OTCAP_CONFIG_BYTES;
    config->frame_limit = frames;
    config->action_count = action_count;
    config->level_index = level;
    memcpy(config->build_sha256, header->build_sha256, OTCAP_BUILD_SHA256_SIZE);
    memcpy(config->actions, actions, sizeof(actions));
    initial->size = OTCAP_INITIAL_STATE_BYTES;

    command_line[0] = 0;
    if (!append_command_argument(command_line, sizeof(command_line), exe_path)) goto cleanup_view;
    SetEnvironmentVariableA(OTCAP_MAPPING_ENV, mapping_name);
    if (!CreateProcessA(0, command_line, 0, 0, FALSE, CREATE_SUSPENDED, 0, 0, &startup, &process)) goto cleanup_view;
    if (!inject_dll(process.hProcess, dll_path)) {
        TerminateProcess(process.hProcess, 1);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        goto cleanup_view;
    }
    if (!wait_for_ready(header, process.hProcess)) {
        TerminateProcess(process.hProcess, 1);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        goto cleanup_view;
    }
    if (ResumeThread(process.hThread) == (DWORD)-1) {
        TerminateProcess(process.hProcess, 1);
    }
    wait_result = wait_for_bounded_capture(header, process.hProcess);
    (void)wait_result;
    if (header->status == OTCAP_STATUS_READY || header->status == OTCAP_STATUS_CAPTURING) {
        if (header->frame_count == header->frame_limit) {
            header->status = OTCAP_STATUS_COMPLETE;
        } else {
            header->status = OTCAP_STATUS_FAILED;
            header->error_code = OTCAP_ERROR_INVALID_CONFIG;
        }
    }
    if (header->bytes_used < header->data_offset || header->bytes_used > OTCAP_MAPPING_SIZE) {
        header->status = OTCAP_STATUS_FAILED;
        header->error_code = OTCAP_ERROR_INVALID_CONFIG;
        goto close_process;
    }
    output = CreateFileA(output_path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (output == INVALID_HANDLE_VALUE) goto close_process;
    if (!WriteFile(output, view, header->bytes_used, &written, 0) || written != header->bytes_used) {
        CloseHandle(output);
        goto close_process;
    }
    CloseHandle(output);
    exit_code = header->status == OTCAP_STATUS_COMPLETE ? 0 : 1;
close_process:
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
cleanup_view:
    UnmapViewOfFile(view);
    CloseHandle(mapping);
done:
    return exit_code;
}
