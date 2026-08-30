#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "capture_layout.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1L)
#endif

static void usage(void) {
    fprintf(stderr,
        "usage: opentony_capture_host.exe --exe FILE --dll FILE --output FILE "
        "--build-sha256 HEX --frames N [--level N] [--force] "
        "[--resume-file FILE] [--action MASK:START:HOLD]...\n");
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
    if (used + length + (used == 0 ? 3 : 4) >= capacity) {
        return 0;
    }
    if (used != 0) {
        command[used++] = ' ';
    }
    command[used++] = '"';
    memcpy(command + used, value, length);
    used += length;
    command[used++] = '"';
    command[used] = 0;
    return 1;
}

static int executable_directory(const char *path, char *directory, size_t capacity) {
    const char *slash;
    size_t length;
    if (path == 0 || directory == 0 || capacity < 2u) {
        return 0;
    }
    slash = strrchr(path, '\\');
    {
        const char *forward = strrchr(path, '/');
        if (forward != 0 && (slash == 0 || forward > slash)) {
            slash = forward;
        }
    }
    if (slash == 0) {
        directory[0] = '.';
        directory[1] = 0;
        return 1;
    }
    length = (size_t)(slash - path);
    if (length == 0u) {
        length = 1u;
    }
    if (length + 1u > capacity) {
        return 0;
    }
    memcpy(directory, path, length);
    directory[length] = 0;
    return 1;
}

static int inject_dll(
    HANDLE process,
    const char *dll_path,
    HANDLE *thread_out,
    LPVOID *remote_path_out) {
    SIZE_T bytes;
    LPVOID remote_path;
    HANDLE thread;
    HMODULE kernel;
    FARPROC loader;
    FARPROC remote_loader;
    bytes = strlen(dll_path) + 1;
    remote_path = VirtualAllocEx(process, 0, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (remote_path == 0 || !WriteProcessMemory(process, remote_path, (void *)dll_path, bytes, &bytes)) {
        fprintf(stderr, "capture host: WriteProcessMemory failed (%lu)\n",
            (unsigned long)GetLastError());
        if (remote_path != 0) VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
        return 0;
    }
    kernel = GetModuleHandleA("kernel32.dll");
    loader = kernel == 0 ? 0 : GetProcAddress(kernel, "LoadLibraryA");
    remote_loader = 0;
    if (loader != 0 && kernel != 0) {
        /* Wine's Toolhelp module snapshot can report ERROR_NO_MORE_FILES for
         * a newly created 32-bit process.  Walk the target's 32-bit PEB
         * loader list instead; the layout is part of the Win32 ABI and is
         * stable for the supported image. */
        typedef LONG (WINAPI *NtQueryInformationProcessFn)(
            HANDLE, DWORD, void *, ULONG, ULONG *);
        typedef struct RemoteProcessBasicInformation {
            void *reserved0;
            void *peb;
            void *reserved1[2];
            DWORD pid;
            void *reserved2;
        } RemoteProcessBasicInformation;
        typedef struct RemoteUnicodeString {
            USHORT length;
            USHORT maximum_length;
            DWORD buffer;
        } RemoteUnicodeString;
        typedef struct RemoteLoaderEntry {
            DWORD flink;
            DWORD blink;
            DWORD reserved[4];
            DWORD dll_base;
            DWORD entry_point;
            DWORD size_of_image;
            RemoteUnicodeString full_name;
            RemoteUnicodeString base_name;
        } RemoteLoaderEntry;
        NtQueryInformationProcessFn query;
        RemoteProcessBasicInformation basic;
        DWORD ldr;
        DWORD list_head;
        DWORD node;
        DWORD offset;
        HMODULE ntdll;
        SIZE_T bytes_read;
        query = 0;
        ntdll = GetModuleHandleA("ntdll.dll");
        if (ntdll != 0) {
            query = (NtQueryInformationProcessFn)GetProcAddress(
                ntdll, "NtQueryInformationProcess");
        }
        ZeroMemory(&basic, sizeof(basic));
        if (query != 0 && query(
                process, 0, &basic, sizeof(basic), 0) == 0 &&
            basic.peb != 0 && ReadProcessMemory(
                process, (unsigned char *)basic.peb + 0x0cu,
                &ldr, sizeof(ldr), &bytes_read) &&
            ReadProcessMemory(
                process, (void *)(ldr + 0x0cu), &list_head,
                sizeof(list_head), &bytes_read)) {
            node = list_head;
            offset = (DWORD)((unsigned char *)loader -
                (unsigned char *)kernel);
            while (node != 0) {
                RemoteLoaderEntry entry;
                char name[32];
                uint32_t index;
                ZeroMemory(&entry, sizeof(entry));
                if (!ReadProcessMemory(
                        process, (void *)node, &entry, sizeof(entry),
                        &bytes_read) || entry.flink == list_head) {
                    /* The head is a list entry, not an image entry.  The
                     * first pass still needs to inspect the node itself. */
                    if (entry.flink == list_head && entry.dll_base == 0) {
                        break;
                    }
                }
                ZeroMemory(name, sizeof(name));
                if (entry.base_name.buffer != 0 &&
                    entry.base_name.length >= 12 &&
                    entry.base_name.length < sizeof(name)) {
                    uint16_t wide[16];
                    uint32_t chars = entry.base_name.length / 2u;
                    if (chars > 15u) chars = 15u;
                    if (ReadProcessMemory(
                            process, (void *)entry.base_name.buffer, wide,
                            chars * 2u, &bytes_read)) {
                        for (index = 0; index < chars; ++index) {
                            uint16_t value = wide[index];
                            name[index] = (char)(value < 0x80u ? value : '?');
                            if (name[index] >= 'A' && name[index] <= 'Z') {
                                name[index] = (char)(name[index] + ('a' - 'A'));
                            }
                        }
                        name[chars] = 0;
                        if (strcmp(name, "kernel32.dll") == 0) {
                            remote_loader = (FARPROC)(entry.dll_base + offset);
                            break;
                        }
                    }
                }
                if (entry.flink == 0 || entry.flink == node) break;
                node = entry.flink;
                if (node == list_head) break;
            }
        }
    }
    if (remote_loader == 0) {
        /* On the supported Wine prefix kernel32 is loaded at the same image
         * base in the host and retail processes.  Keep the RVA walk as the
         * preferred path, but retain this ABI-compatible fallback for the
         * restricted ReadProcessMemory policy used by some Wine builds. */
        remote_loader = loader;
    }
    thread = remote_loader == 0 ? 0 : CreateRemoteThread(process, 0, 0,
        (LPTHREAD_START_ROUTINE)remote_loader, remote_path, 0, 0);
    if (thread == 0) {
        fprintf(stderr, "capture host: CreateRemoteThread failed (%lu)\n",
            (unsigned long)GetLastError());
        VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
        return 0;
    }
    *thread_out = thread;
    *remote_path_out = remote_path;
    return 1;
}

static int wait_for_ready(CaptureHeader *header, HANDLE process) {
    uint32_t elapsed;
    (void)process;
    for (elapsed = 0; elapsed < 10000u; ++elapsed) {
        if (header->status == OTCAP_STATUS_READY) {
            return 1;
        }
        if (header->status == OTCAP_STATUS_FAILED || header->status == OTCAP_STATUS_OVERFLOW) {
            return 0;
        }
        Sleep(1);
    }
    return 0;
}

static int wait_for_resume_file(const char *path) {
    DWORD elapsed;
    if (path == 0 || *path == 0) {
        return 1;
    }
    for (elapsed = 0; elapsed < 120000u; ++elapsed) {
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
            return 1;
        }
        Sleep(1);
    }
    return 0;
}

static DWORD wait_for_bounded_capture(CaptureHeader *header, HANDLE process) {
    DWORD start = GetTickCount();
    for (;;) {
        DWORD result = WaitForSingleObject(process, 50);
        if (result == WAIT_OBJECT_0 || result == WAIT_FAILED) {
            return result;
        }
        if (header->status == OTCAP_STATUS_FAILED ||
            header->status == OTCAP_STATUS_OVERFLOW) {
            TerminateProcess(process, 1);
            return WaitForSingleObject(process, 5000u);
        }
        if (header->status == OTCAP_STATUS_COMPLETE ||
            header->frame_count >= header->frame_limit) {
            /* The final record is published before frame_count advances.  A
             * bounded capture therefore has no reason to leave the retail
             * frontend running (and cannot depend on a debugger-issued quit). */
            InterlockedExchange((LONG *)&header->status, OTCAP_STATUS_COMPLETE);
            TerminateProcess(process, 0);
            return WaitForSingleObject(process, 5000u);
        }
        /* A retail frontend that never reaches gameplay must not leave the
         * launcher (or its Xvfb display) alive indefinitely.  The timeout is
         * deliberately generous for startup/movie loading, but bounded for
         * automation and fail-closed rather than yielding a partial capture. */
        if ((DWORD)(GetTickCount() - start) >= 120000u) {
            InterlockedExchange((LONG *)&header->error_code, OTCAP_ERROR_TIMEOUT);
            InterlockedExchange((LONG *)&header->status, OTCAP_STATUS_FAILED);
            TerminateProcess(process, 1);
            return WaitForSingleObject(process, 5000u);
        }
    }
}

int main(int argc, char **argv) {
    const char *exe_path = 0;
    const char *dll_path = 0;
    const char *output_path = 0;
    const char *sha_text = 0;
    const char *resume_file = 0;
    char mapping_name[128];
    char command_line[2048];
    char working_directory[MAX_PATH];
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
    uint32_t max_frames = 0;
    int force = 0;
    CaptureActionInterval actions[OTCAP_MAX_ACTION_INTERVALS];
    uint32_t index;
    DWORD wait_result;
    HANDLE output;
    HANDLE injection_thread = 0;
    LPVOID injection_path = 0;
    DWORD written;
    int exit_code = 1;

    /* Wine and the Windows CRT may otherwise retain diagnostics until the
     * suspended child is torn down, hiding the fail-closed reason from the
     * launcher.  The host is a short-lived diagnostic boundary, so make its
     * status observable immediately. */
    setvbuf(stderr, 0, _IONBF, 0);

    ZeroMemory(&startup, sizeof(startup));
    startup.cb = sizeof(startup);
    working_directory[0] = 0;
    ZeroMemory(&process, sizeof(process));
    ZeroMemory(actions, sizeof(actions));
    for (index = 1; index < (uint32_t)argc; ++index) {
        if (strcmp(argv[index], "--exe") == 0 && index + 1 < (uint32_t)argc) exe_path = argv[++index];
        else if (strcmp(argv[index], "--dll") == 0 && index + 1 < (uint32_t)argc) dll_path = argv[++index];
        else if (strcmp(argv[index], "--output") == 0 && index + 1 < (uint32_t)argc) output_path = argv[++index];
        else if (strcmp(argv[index], "--build-sha256") == 0 && index + 1 < (uint32_t)argc) sha_text = argv[++index];
        else if (strcmp(argv[index], "--frames") == 0 && index + 1 < (uint32_t)argc) { if (!parse_u32(argv[++index], &frames)) goto done; }
        else if (strcmp(argv[index], "--level") == 0 && index + 1 < (uint32_t)argc) { if (!parse_u32(argv[++index], &level)) goto done; }
        else if (strcmp(argv[index], "--resume-file") == 0 && index + 1 < (uint32_t)argc) resume_file = argv[++index];
        else if (strcmp(argv[index], "--force") == 0) force = 1;
        else if (strcmp(argv[index], "--action") == 0 && index + 1 < (uint32_t)argc) {
            if (action_count >= OTCAP_MAX_ACTION_INTERVALS) goto done;
            if (!parse_action(argv[++index], &actions[action_count])) goto done;
            ++action_count;
        } else { usage(); goto done; }
    }
    if (exe_path == 0 || dll_path == 0 || output_path == 0 || sha_text == 0 || frames == 0) {
        usage();
        goto done;
    }
    if (!executable_directory(exe_path, working_directory, sizeof(working_directory))) {
        fprintf(stderr, "capture host: executable path is too long\n");
        goto done;
    }
    if (!force && GetFileAttributesA(output_path) != INVALID_FILE_ATTRIBUTES) {
        fprintf(stderr, "refusing to overwrite %s; pass --force\n", output_path);
        goto done;
    }
    mapping_name[0] = 0;
    _snprintf(mapping_name, sizeof(mapping_name) - 1, "Local\\OpenTonyCapture-%lu", (unsigned long)GetCurrentProcessId());
    mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, 0, OTCAP_MAPPING_SIZE, mapping_name);
    if (mapping == 0) {
        fprintf(stderr, "capture host: CreateFileMappingA failed (%lu)\n", (unsigned long)GetLastError());
        goto done;
    }
    view = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, OTCAP_MAPPING_SIZE);
    if (view == 0) {
        fprintf(stderr, "capture host: MapViewOfFile failed (%lu)\n", (unsigned long)GetLastError());
        CloseHandle(mapping);
        goto done;
    }
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
    max_frames = otcap_max_frames(header->mapping_size, header->data_offset);
    if (frames > max_frames) {
        fprintf(stderr, "capture host: requested %lu frames exceeds mapping capacity %lu\n",
            (unsigned long)frames, (unsigned long)max_frames);
        goto cleanup_view;
    }
    header->bytes_used = header->data_offset;
    header->frame_limit = frames;
    header->level_index = level;
    header->image_base = 0x00400000u;
    header->player_blob_size = OTCAP_PLAYER_BLOB_SIZE;
    header->process_id = GetCurrentProcessId();
    if (!parse_sha256(sha_text, header->build_sha256)) {
        fprintf(stderr, "capture host: invalid --build-sha256\n");
        goto cleanup_view;
    }
    config->version = OTCAP_VERSION;
    config->size = OTCAP_CONFIG_BYTES;
    config->frame_limit = frames;
    config->action_count = action_count;
    config->level_index = level;
    memcpy(config->build_sha256, header->build_sha256, OTCAP_BUILD_SHA256_SIZE);
    memcpy(config->actions, actions, sizeof(actions));
    initial->size = OTCAP_INITIAL_STATE_BYTES;

    command_line[0] = 0;
    if (!append_command_argument(command_line, sizeof(command_line), exe_path)) {
        fprintf(stderr, "capture host: executable command line is too long\n");
        goto cleanup_view;
    }
    if (!SetEnvironmentVariableA(OTCAP_MAPPING_ENV, mapping_name)) {
        fprintf(stderr, "capture host: SetEnvironmentVariableA failed (%lu)\n", (unsigned long)GetLastError());
        goto cleanup_view;
    }
    if (!CreateProcessA(exe_path, command_line, 0, 0, FALSE, CREATE_SUSPENDED, 0,
            working_directory, &startup, &process)) {
        fprintf(stderr, "capture host: CreateProcessA failed (%lu)\n", (unsigned long)GetLastError());
        goto cleanup_view;
    }
    header->process_id = process.dwProcessId;
    if (!inject_dll(
            process.hProcess, dll_path,
            &injection_thread, &injection_path)) {
        fprintf(stderr, "capture host: DLL injection failed\n");
        TerminateProcess(process.hProcess, 1);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        goto cleanup_view;
    }
    if (!wait_for_resume_file(resume_file)) {
        fprintf(stderr, "capture host: resume rendezvous timed out\n");
        InterlockedExchange((LONG *)&header->error_code, OTCAP_ERROR_TIMEOUT);
        InterlockedExchange((LONG *)&header->status, OTCAP_STATUS_FAILED);
        TerminateProcess(process.hProcess, 1);
        CloseHandle(injection_thread);
        VirtualFreeEx(process.hProcess, injection_path, 0, MEM_RELEASE);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        goto cleanup_view;
    }
    /* Let the primary loader thread release its loader lock before waiting
     * for the remote LoadLibraryA call.  Waiting while CREATE_SUSPENDED is
     * active deadlocks under Wine (and is unnecessary on Windows). */
    if (ResumeThread(process.hThread) == (DWORD)-1) {
        fprintf(stderr, "capture host: ResumeThread failed (%lu)\n", (unsigned long)GetLastError());
        TerminateProcess(process.hProcess, 1);
        CloseHandle(injection_thread);
        VirtualFreeEx(process.hProcess, injection_path, 0, MEM_RELEASE);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        goto cleanup_view;
    }
    /* Do not wait on the loader thread here.  Wine may keep that wait handle
     * unsignaled even after the DLL has initialized its mapping; READY is the
     * recorder-level synchronization boundary. */
    CloseHandle(injection_thread);
    injection_thread = 0;
    if (!wait_for_ready(header, process.hProcess)) {
        fprintf(stderr, "capture host: recorder did not become ready (status=%lu, error=%lu)\n",
            (unsigned long)header->status, (unsigned long)header->error_code);
        TerminateProcess(process.hProcess, 1);
        VirtualFreeEx(process.hProcess, injection_path, 0, MEM_RELEASE);
        injection_path = 0;
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        goto cleanup_view;
    }
    wait_result = wait_for_bounded_capture(header, process.hProcess);
    (void)wait_result;
    /* The process is either terminated by the bounded waiter or has exited;
     * the remote argument is no longer needed. */
    VirtualFreeEx(process.hProcess, injection_path, 0, MEM_RELEASE);
    injection_path = 0;
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
    if (output == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "capture host: output open failed (%lu)\n", (unsigned long)GetLastError());
        goto close_process;
    }
    if (!WriteFile(output, view, header->bytes_used, &written, 0) || written != header->bytes_used) {
        fprintf(stderr, "capture host: output write failed (%lu)\n", (unsigned long)GetLastError());
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
