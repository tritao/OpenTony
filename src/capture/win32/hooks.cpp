#include "hooks.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string.h>

/* These globals are deliberately absolute for the one verified image.  The
 * build check rejects rebased/unknown images before this code can run. */
#define OTCAP_PLAYER_TABLE_ADDRESS 0x0056a858u
#define OTCAP_ACTION_MASK_ADDRESS 0x006a3f1cu
#define OTCAP_AIR_CONTROL_ADDRESS 0x0056b7f0u
#define OTCAP_ANIMATION_CLOCK_ADDRESS 0x005685f4u
#define OTCAP_ANIMATION_SCALE_ADDRESS 0x0056865cu
#define OTCAP_ANIMATION_SCALE_SQUARE_ADDRESS 0x00568804u
#define OTCAP_ANIMATION_CLOCK_ACCUMULATOR_ADDRESS 0x00568810u
#define OTCAP_SIMULATION_TIME_ADDRESS 0x0056e320u
#define OTCAP_TIMING_DELTA_ADDRESS 0x0056a93cu
#define OTCAP_TIMER_STATE_ADDRESS 0x006a05a0u
#define OTCAP_TIMER_PUBLIC_ACCUMULATOR_ADDRESS 0x006a0590u
#define OTCAP_TIMER_SIMULATION_ACCUMULATOR_ADDRESS 0x006a0598u
#define OTCAP_TIMER_PUBLIC_TICK_ADDRESS 0x0056e31cu
#define OTCAP_TIMER_SIMULATION_TIME_ADDRESS 0x0056e320u
#define OTCAP_TIMER_PAUSE_GATE_A_ADDRESS 0x00561c04u
#define OTCAP_TIMER_PAUSE_GATE_B_ADDRESS 0x0056a8e0u

static CaptureBuffer *g_capture_buffer = 0;
static LONG g_physics_frame_active = 0;
static uint32_t g_physics_frame_index = 0;
static uint32_t g_physics_player = 0;
static uint32_t g_physics_input_mask = 0;
static uint32_t g_physics_input_flags = 0;
static uint8_t g_physics_before[OTCAP_PLAYER_BLOB_SIZE];
static CaptureTimingSnapshot g_physics_timing_before;
static void *g_physics_target = 0;
static void *g_physics_trampoline = 0;
static uint8_t g_physics_original[16];
static uint32_t g_physics_overwrite_size = 0;
static int g_physics_installed = 0;
static void *g_input_target = 0;
static void *g_input_trampoline = 0;
static uint8_t g_input_original[16];
static uint32_t g_input_overwrite_size = 0;
static volatile uint32_t g_input_injected_frame = 0xffffffffu;
static int g_input_installed = 0;
static CaptureTimerSample g_pending_timer_samples[OTCAP_MAX_TIMER_SAMPLES];
static uint32_t g_pending_timer_count = 0;
static uint32_t g_pending_timer_frame = 0xffffffffu;
static CaptureTimerSample g_frame_timer_samples[OTCAP_MAX_TIMER_SAMPLES];
static uint32_t g_frame_timer_count = 0;
static int g_initial_timer_captured = 0;
static void *g_timer_target = 0;
static void *g_timer_trampoline = 0;
static uint8_t g_timer_original[16];
static uint32_t g_timer_overwrite_size = 0;
static int g_timer_installed = 0;
static void *g_clock_read_target = 0;
static void *g_clock_read_trampoline = 0;
static uint8_t g_clock_read_original[16];
static uint32_t g_clock_read_overwrite_size = 0;
static int g_clock_read_installed = 0;

typedef struct CaptureSha256 {
    uint32_t state[8];
    uint64_t bits;
    uint8_t block[64];
    uint32_t block_size;
} CaptureSha256;

static const uint32_t k_sha256_constants[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b,
    0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
    0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7,
    0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152,
    0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
    0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
    0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
    0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
    0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

static uint32_t sha_rotr(uint32_t value, uint32_t count) {
    return (value >> count) | (value << (32u - count));
}

static uint32_t sha_load32(const uint8_t *bytes) {
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
        ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

static void sha_store32(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)(value >> 24);
    bytes[1] = (uint8_t)(value >> 16);
    bytes[2] = (uint8_t)(value >> 8);
    bytes[3] = (uint8_t)value;
}

static void sha_transform(CaptureSha256 *sha, const uint8_t *block) {
    uint32_t words[64];
    uint32_t a, b, c, d, e, f, g, h;
    uint32_t index;
    for (index = 0; index < 16; ++index) {
        words[index] = sha_load32(block + index * 4);
    }
    for (index = 16; index < 64; ++index) {
        uint32_t s0 = sha_rotr(words[index - 15], 7) ^ sha_rotr(words[index - 15], 18) ^ (words[index - 15] >> 3);
        uint32_t s1 = sha_rotr(words[index - 2], 17) ^ sha_rotr(words[index - 2], 19) ^ (words[index - 2] >> 10);
        words[index] = words[index - 16] + s0 + words[index - 7] + s1;
    }
    a = sha->state[0]; b = sha->state[1]; c = sha->state[2]; d = sha->state[3];
    e = sha->state[4]; f = sha->state[5]; g = sha->state[6]; h = sha->state[7];
    for (index = 0; index < 64; ++index) {
        uint32_t s1 = sha_rotr(e, 6) ^ sha_rotr(e, 11) ^ sha_rotr(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + s1 + ch + k_sha256_constants[index] + words[index];
        uint32_t s0 = sha_rotr(a, 2) ^ sha_rotr(a, 13) ^ sha_rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + maj;
        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }
    sha->state[0] += a; sha->state[1] += b; sha->state[2] += c; sha->state[3] += d;
    sha->state[4] += e; sha->state[5] += f; sha->state[6] += g; sha->state[7] += h;
}

static void sha_init(CaptureSha256 *sha) {
    sha->state[0] = 0x6a09e667; sha->state[1] = 0xbb67ae85;
    sha->state[2] = 0x3c6ef372; sha->state[3] = 0xa54ff53a;
    sha->state[4] = 0x510e527f; sha->state[5] = 0x9b05688c;
    sha->state[6] = 0x1f83d9ab; sha->state[7] = 0x5be0cd19;
    sha->bits = 0;
    sha->block_size = 0;
}

static void sha_update(CaptureSha256 *sha, const uint8_t *bytes, uint32_t size) {
    uint32_t count;
    sha->bits += (uint64_t)size * 8u;
    while (size != 0) {
        count = 64u - sha->block_size;
        if (count > size) count = size;
        memcpy(sha->block + sha->block_size, bytes, count);
        sha->block_size += count;
        bytes += count;
        size -= count;
        if (sha->block_size == 64u) {
            sha_transform(sha, sha->block);
            sha->block_size = 0;
        }
    }
}

static void sha_final(CaptureSha256 *sha, uint8_t output[32]) {
    uint32_t index;
    uint64_t bits = sha->bits;
    sha->block[sha->block_size++] = 0x80;
    while (sha->block_size != 56u) {
        if (sha->block_size == 64u) {
            sha_transform(sha, sha->block);
            sha->block_size = 0;
        }
        sha->block[sha->block_size++] = 0;
    }
    for (index = 0; index < 8; ++index) {
        sha->block[56u + index] = (uint8_t)(bits >> (56u - index * 8u));
    }
    sha_transform(sha, sha->block);
    for (index = 0; index < 8; ++index) {
        sha_store32(output + index * 4, sha->state[index]);
    }
}

static int sha_file(const char *path, uint8_t output[32]) {
    HANDLE file;
    DWORD read;
    uint8_t buffer[4096];
    CaptureSha256 sha;
    file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE) return 0;
    sha_init(&sha);
    for (;;) {
        if (!ReadFile(file, buffer, sizeof(buffer), &read, 0)) {
            CloseHandle(file);
            return 0;
        }
        if (read == 0) break;
        sha_update(&sha, buffer, read);
    }
    CloseHandle(file);
    sha_final(&sha, output);
    return 1;
}

/*
 * Addresses are RVAs from the supported THawk2 image base 0x00400000.  The
 * expected bytes are read from the immutable executable and are checked again
 * in the live process before any future detour is allowed.
 */
static const CaptureHookSpec k_hooks[] = {
    {
        "physics_frame",
        0x0009e680,
        {0x81, 0xec, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00},
        6,
        6,
    },
    {
        "action_mask_injection",
        0x00089a15,
        {0xa0, 0xb9, 0xaf, 0x56, 0x00, 0x00, 0x00, 0x00},
        5,
        5,
    },
    {
        "input_boundary",
        0x00069de0,
        {0x57, 0xe8, 0x4a, 0xfc, 0xff, 0xff, 0x00, 0x00},
        6,
        6,
    },
    {
        "timer_update",
        0x0006a0f0,
        {0xe8, 0x3b, 0xea, 0xff, 0xff, 0xe8, 0x26, 0xe4, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        10,
        10,
    },
    {
        "clock_read",
        0x0009f1a0,
        {0x8b, 0x15, 0x20, 0xe3, 0x56, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        6,
        6,
    },
};

const CaptureHookSpec *ot_capture_hook_manifest(uint32_t *count) {
    if (count != 0) {
        *count = (uint32_t)(sizeof(k_hooks) / sizeof(k_hooks[0]));
    }
    return k_hooks;
}

int ot_capture_verify_build(void *module_base, const uint8_t expected_sha256[32]) {
    /*
     * The no-CD copy has a different file hash, but preserves this PE identity
     * and all recorder seams.  The source hash is still carried in the output
     * and checked by the host before launch.
     */
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)module_base;
    IMAGE_NT_HEADERS *nt;
    uint8_t actual_sha[32];
    char module_path[MAX_PATH];
    DWORD module_path_size;
    static const uint8_t supported_sha[32] = {
        0xf2, 0xc7, 0xca, 0x7c, 0xbc, 0x31, 0xab, 0xd8,
        0xf7, 0x48, 0xbd, 0x4a, 0xfd, 0xc1, 0xe3, 0x0a,
        0xa1, 0xa6, 0x70, 0x0c, 0xe9, 0x18, 0x93, 0xb6,
        0x18, 0x45, 0x0f, 0xd1, 0x61, 0x72, 0x66, 0x9c,
    };
    static const uint8_t supported_nocd_sha[32] = {
        0x03, 0xd5, 0xba, 0x74, 0xdb, 0xc9, 0x09, 0xe3,
        0xa4, 0x17, 0xf1, 0xf5, 0xb8, 0x54, 0xa8, 0x36,
        0xc8, 0xc9, 0xe8, 0x9b, 0xd3, 0x1e, 0x9d, 0x1c,
        0x33, 0x1f, 0x01, 0xab, 0x61, 0x3a, 0x94, 0x78,
    };
    if (module_base == 0 || expected_sha256 == 0 || dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return 0;
    }
    if (memcmp(expected_sha256, supported_sha, sizeof(supported_sha)) != 0) {
        return 0;
    }
    nt = (IMAGE_NT_HEADERS *)((unsigned char *)module_base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        nt->OptionalHeader.ImageBase != 0x00400000u ||
        nt->FileHeader.TimeDateStamp != 978633206u) {
        return 0;
    }
    module_path_size = GetModuleFileNameA(0, module_path, sizeof(module_path));
    if (module_path_size == 0 || module_path_size >= sizeof(module_path) ||
        !sha_file(module_path, actual_sha) ||
        (memcmp(actual_sha, supported_sha, sizeof(supported_sha)) != 0 &&
         memcmp(actual_sha, supported_nocd_sha, sizeof(supported_nocd_sha)) != 0)) {
        return 0;
    }
    return 1;
}

int ot_capture_verify_hooks(void *module_base, uint32_t *failed_index) {
    uint32_t count = 0;
    uint32_t index;
    const CaptureHookSpec *hooks = ot_capture_hook_manifest(&count);
    if (module_base == 0) {
        if (failed_index != 0) {
            *failed_index = 0;
        }
        return 0;
    }
    for (index = 0; index < count; ++index) {
        const unsigned char *address =
            (const unsigned char *)module_base + hooks[index].rva;
        if (memcmp(address, hooks[index].expected, hooks[index].expected_size) != 0) {
            if (failed_index != 0) {
                *failed_index = index;
            }
            return 0;
        }
    }
    return 1;
}

static const CaptureHookSpec *find_hook(const char *name) {
    uint32_t count = 0;
    uint32_t index;
    const CaptureHookSpec *hooks = ot_capture_hook_manifest(&count);
    for (index = 0; index < count; ++index) {
        if (strcmp(hooks[index].name, name) == 0) {
            return &hooks[index];
        }
    }
    return 0;
}

int ot_capture_bind_buffer(CaptureBuffer *buffer) {
    if (buffer == 0 || buffer->mapping == 0 || buffer->mapping_size == 0 ||
        g_physics_installed || g_input_installed || g_timer_installed ||
        g_clock_read_installed) {
        return 0;
    }
    g_capture_buffer = buffer;
    return 1;
}

static int copy_player_and_timing(
    uint32_t player,
    uint8_t *destination,
    CaptureTimingSnapshot *timing) {
    if (player == 0 || destination == 0 || timing == 0) {
        return 0;
    }
#if defined(_MSC_VER)
    __try {
#endif
        memcpy(destination, (const void *)player, OTCAP_PLAYER_BLOB_SIZE);
        timing->animation_clock =
            *(volatile uint32_t *)OTCAP_ANIMATION_CLOCK_ADDRESS;
        timing->animation_time_scale =
            *(volatile uint32_t *)OTCAP_ANIMATION_SCALE_ADDRESS;
        timing->animation_time_scale_square =
            *(volatile uint32_t *)OTCAP_ANIMATION_SCALE_SQUARE_ADDRESS;
        timing->animation_clock_accumulator =
            *(volatile uint32_t *)OTCAP_ANIMATION_CLOCK_ACCUMULATOR_ADDRESS;
        timing->simulation_time =
            *(volatile uint32_t *)OTCAP_SIMULATION_TIME_ADDRESS;
        timing->timing_delta_q11 =
            *(volatile uint32_t *)OTCAP_TIMING_DELTA_ADDRESS;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
#endif
    return 1;
}

static int read_timer_sample(
    uint32_t phase,
    uint32_t frame_index,
    CaptureTimerSample *sample) {
    if (sample == 0) {
        return 0;
    }
#if defined(_MSC_VER)
    __try {
#endif
        ZeroMemory(sample, sizeof(*sample));
        sample->phase = phase;
        sample->frame_index = frame_index;
        sample->interval_ms =
            *(volatile uint32_t *)(OTCAP_TIMER_STATE_ADDRESS + 0x04u);
        sample->accumulated_ms =
            *(volatile uint32_t *)(OTCAP_TIMER_STATE_ADDRESS + 0x0cu);
        sample->public_tick =
            *(volatile uint32_t *)OTCAP_TIMER_PUBLIC_TICK_ADDRESS;
        sample->simulation_time =
            *(volatile uint32_t *)OTCAP_TIMER_SIMULATION_TIME_ADDRESS;
        sample->pause_gate_a =
            *(volatile uint32_t *)OTCAP_TIMER_PAUSE_GATE_A_ADDRESS;
        sample->pause_gate_b =
            *(volatile uint32_t *)OTCAP_TIMER_PAUSE_GATE_B_ADDRESS;
        memcpy(
            &sample->public_accumulator_raw,
            (const void *)OTCAP_TIMER_PUBLIC_ACCUMULATOR_ADDRESS,
            sizeof(sample->public_accumulator_raw));
        memcpy(
            &sample->simulation_accumulator_raw,
            (const void *)OTCAP_TIMER_SIMULATION_ACCUMULATOR_ADDRESS,
            sizeof(sample->simulation_accumulator_raw));
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
#endif
    return 1;
}

static int capture_initial_timer_state(void) {
    CaptureHeader *header;
    CaptureInitialState *initial;

    if (g_capture_buffer == 0 || g_capture_buffer->mapping == 0) {
        return 0;
    }
    if (g_initial_timer_captured) {
        return 1;
    }
    header = (CaptureHeader *)g_capture_buffer->mapping;
    initial = (CaptureInitialState *)(
        g_capture_buffer->mapping + header->initial_state_offset);
#if defined(_MSC_VER)
    __try {
#endif
        initial->timer_handle =
            *(volatile uint32_t *)(OTCAP_TIMER_STATE_ADDRESS + 0x00u);
        initial->interval_ms =
            *(volatile uint32_t *)(OTCAP_TIMER_STATE_ADDRESS + 0x04u);
        initial->opaque_08 =
            *(volatile uint32_t *)(OTCAP_TIMER_STATE_ADDRESS + 0x08u);
        initial->accumulated_ms =
            *(volatile uint32_t *)(OTCAP_TIMER_STATE_ADDRESS + 0x0cu);
        initial->opaque_10 =
            *(volatile uint32_t *)(OTCAP_TIMER_STATE_ADDRESS + 0x10u);
        memcpy(
            &initial->public_accumulator_raw,
            (const void *)OTCAP_TIMER_PUBLIC_ACCUMULATOR_ADDRESS,
            sizeof(initial->public_accumulator_raw));
        memcpy(
            &initial->simulation_accumulator_raw,
            (const void *)OTCAP_TIMER_SIMULATION_ACCUMULATOR_ADDRESS,
            sizeof(initial->simulation_accumulator_raw));
        initial->public_tick =
            *(volatile uint32_t *)OTCAP_TIMER_PUBLIC_TICK_ADDRESS;
        initial->simulation_time =
            *(volatile uint32_t *)OTCAP_TIMER_SIMULATION_TIME_ADDRESS;
        initial->simulation_pause_gate_a =
            *(volatile uint32_t *)OTCAP_TIMER_PAUSE_GATE_A_ADDRESS;
        initial->simulation_pause_gate_b =
            *(volatile uint32_t *)OTCAP_TIMER_PAUSE_GATE_B_ADDRESS;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
#endif
    g_initial_timer_captured = 1;
    return 1;
}

static int append_pending_timer_sample(uint32_t phase, uint32_t frame_index) {
    if (g_pending_timer_count >= OTCAP_MAX_TIMER_SAMPLES) {
        ot_capture_buffer_fail(
            g_capture_buffer, OTCAP_STATUS_OVERFLOW, OTCAP_ERROR_OVERFLOW);
        return 0;
    }
    if (g_pending_timer_count == 0) {
        g_pending_timer_frame = frame_index;
    } else if (g_pending_timer_frame != frame_index) {
        ot_capture_buffer_fail(
            g_capture_buffer, OTCAP_STATUS_OVERFLOW, OTCAP_ERROR_OVERFLOW);
        return 0;
    }
    if (!read_timer_sample(
            phase, frame_index, &g_pending_timer_samples[g_pending_timer_count])) {
        ot_capture_buffer_fail(
            g_capture_buffer, OTCAP_STATUS_FAILED, OTCAP_ERROR_SNAPSHOT);
        return 0;
    }
    ++g_pending_timer_count;
    return 1;
}

static int drain_pending_timer_samples(uint32_t frame_index) {
    uint32_t index;
    if (g_pending_timer_count == 0) {
        return 1;
    }
    if (g_pending_timer_frame != frame_index ||
        g_frame_timer_count + g_pending_timer_count > OTCAP_MAX_TIMER_SAMPLES) {
        ot_capture_buffer_fail(
            g_capture_buffer, OTCAP_STATUS_OVERFLOW, OTCAP_ERROR_OVERFLOW);
        return 0;
    }
    for (index = 0; index < g_pending_timer_count; ++index) {
        g_frame_timer_samples[g_frame_timer_count++] =
            g_pending_timer_samples[index];
    }
    g_pending_timer_count = 0;
    g_pending_timer_frame = 0xffffffffu;
    return 1;
}

static int append_frame_timer_sample(uint32_t phase, uint32_t frame_index) {
    if (g_frame_timer_count >= OTCAP_MAX_TIMER_SAMPLES ||
        !read_timer_sample(
            phase, frame_index, &g_frame_timer_samples[g_frame_timer_count])) {
        ot_capture_buffer_fail(
            g_capture_buffer,
            g_frame_timer_count >= OTCAP_MAX_TIMER_SAMPLES ?
                OTCAP_STATUS_OVERFLOW : OTCAP_STATUS_FAILED,
            g_frame_timer_count >= OTCAP_MAX_TIMER_SAMPLES ?
                OTCAP_ERROR_OVERFLOW : OTCAP_ERROR_SNAPSHOT);
        return 0;
    }
    ++g_frame_timer_count;
    return 1;
}

void __cdecl ot_capture_physics_before(uint32_t player) {
    CaptureHeader *header;
    uint32_t current_player;

    if (g_capture_buffer == 0 || g_capture_buffer->mapping == 0 ||
        g_physics_frame_active != 0) {
        return;
    }
    /* The wrapper runs on the single gameplay thread.  Avoid the VC6-era
     * InterlockedCompareExchange pointer overload and keep this state local
     * to that deterministic boundary. */
    g_physics_frame_active = 1;
    header = (CaptureHeader *)g_capture_buffer->mapping;
    if (header->status != OTCAP_STATUS_READY &&
        header->status != OTCAP_STATUS_CAPTURING) {
        g_physics_frame_active = 0;
        return;
    }
    if (header->frame_count >= header->frame_limit) {
        ot_capture_buffer_fail(
            g_capture_buffer, OTCAP_STATUS_OVERFLOW, OTCAP_ERROR_OVERFLOW);
        g_physics_frame_active = 0;
        return;
    }
    g_physics_frame_index = header->frame_count;
    g_frame_timer_count = 0;
    if (!capture_initial_timer_state() ||
        !drain_pending_timer_samples(g_physics_frame_index) ||
        !append_frame_timer_sample(
            OTCAP_TIMER_PHASE_PHYSICS_ENTRY, g_physics_frame_index)) {
        g_physics_frame_active = 0;
        return;
    }

#if defined(_MSC_VER)
    __try {
#endif
        current_player = *(volatile uint32_t *)OTCAP_PLAYER_TABLE_ADDRESS;
        /* Skater_PhysicsFrame is also used for the second skater.  Canonical
         * recordings intentionally retain only table entry zero, matching the
         * GDB recorder's player filter. */
        if (player != current_player ||
            !copy_player_and_timing(
                player, g_physics_before, &g_physics_timing_before)) {
            g_physics_frame_active = 0;
            return;
        }
        g_physics_player = player;
        g_physics_input_mask =
            *(volatile uint32_t *)OTCAP_ACTION_MASK_ADDRESS;
        g_physics_input_flags =
            (*(volatile uint32_t *)OTCAP_AIR_CONTROL_ADDRESS ?
                OTCAP_INPUT_FLAG_AIR_CONTROL : 0u) |
            (g_input_injected_frame == header->frame_count ?
                OTCAP_INPUT_FLAG_INJECTED : 0u);
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ot_capture_buffer_fail(
            g_capture_buffer, OTCAP_STATUS_FAILED, OTCAP_ERROR_SNAPSHOT);
        g_physics_frame_active = 0;
        return;
    }
#endif
    InterlockedExchange((LONG *)&header->status, OTCAP_STATUS_CAPTURING);
}

void __cdecl ot_capture_physics_after(void) {
    CaptureHeader *header;
    uint8_t after[OTCAP_PLAYER_BLOB_SIZE];
    CaptureTimingSnapshot timing_after;

    if (g_capture_buffer == 0 || g_capture_buffer->mapping == 0 ||
        g_physics_frame_active == 0) {
        return;
    }
    header = (CaptureHeader *)g_capture_buffer->mapping;
    if (!drain_pending_timer_samples(g_physics_frame_index) ||
        !append_frame_timer_sample(
            OTCAP_TIMER_PHASE_POST_PHYSICS, g_physics_frame_index)) {
        g_physics_frame_active = 0;
        return;
    }
    if (!copy_player_and_timing(
            g_physics_player, after, &timing_after) ||
        !ot_capture_append_frame(
            g_capture_buffer,
            g_physics_frame_index,
            g_physics_input_mask,
            g_physics_input_flags,
            g_physics_player,
            g_physics_before,
            after,
            &g_physics_timing_before,
            &timing_after,
            g_frame_timer_samples,
            g_frame_timer_count)) {
        ot_capture_buffer_fail(
            g_capture_buffer, OTCAP_STATUS_FAILED, OTCAP_ERROR_SNAPSHOT);
        g_physics_frame_active = 0;
        return;
    }
    if (header->frame_count >= header->frame_limit) {
        /* The host observes this published state and ends the bounded retail
         * process after the final complete record has been committed. */
        InterlockedExchange((LONG *)&header->status, OTCAP_STATUS_COMPLETE);
    }
    g_physics_frame_active = 0;
}

void __cdecl ot_capture_input_boundary(void) {
    CaptureHeader *header;
    CaptureConfig *config;
    uint32_t frame_index;
    uint32_t action_mask;

    if (g_capture_buffer == 0 || g_capture_buffer->mapping == 0) {
        return;
    }
    header = (CaptureHeader *)g_capture_buffer->mapping;
    if (header->status != OTCAP_STATUS_READY &&
        header->status != OTCAP_STATUS_CAPTURING) {
        return;
    }
    frame_index = header->frame_count;
    config = (CaptureConfig *)(g_capture_buffer->mapping + header->config_offset);
    action_mask = ot_capture_action_mask(config, frame_index);
    if (action_mask == 0) {
        return;
    }
#if defined(_MSC_VER)
    __try {
#endif
        /* GDB's action-edge probe writes exactly the low word after the live
         * poll/build boundary.  Preserve the untouched high word. */
        *(volatile uint16_t *)OTCAP_ACTION_MASK_ADDRESS =
            (uint16_t)action_mask;
        g_input_injected_frame = frame_index;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ot_capture_buffer_fail(
            g_capture_buffer, OTCAP_STATUS_FAILED, OTCAP_ERROR_SNAPSHOT);
    }
#endif
}

void __cdecl ot_capture_timer_boundary(uint32_t phase) {
    CaptureHeader *header;
    uint32_t frame_index;

    if (g_capture_buffer == 0 || g_capture_buffer->mapping == 0) {
        return;
    }
    header = (CaptureHeader *)g_capture_buffer->mapping;
    if (header->status != OTCAP_STATUS_READY &&
        header->status != OTCAP_STATUS_CAPTURING) {
        return;
    }
    frame_index = header->frame_count;
    if (frame_index >= header->frame_limit) {
        return;
    }
    if (!capture_initial_timer_state() ||
        !append_pending_timer_sample(phase, frame_index)) {
        return;
    }
}

#if defined(_MSC_VER) && defined(_M_IX86)
/* The function has a six-byte prologue.  The naked wrapper keeps the original
 * ECX/stack ABI, calls the trampoline once, and restores the return registers
 * after the observer runs. */
static __declspec(naked) void ot_capture_physics_hook(void) {
    __asm {
        pushad
        pushfd
        push ecx
        call ot_capture_physics_before
        add esp, 4
        popfd
        popad
        call dword ptr [g_physics_trampoline]
        pushad
        pushfd
        call ot_capture_physics_after
        popfd
        popad
        ret
    }
}

/* This is a mid-function boundary rather than a callable function entry.  It
 * therefore jumps through the trampoline after the observer; no return is
 * pushed and the original five-byte load executes in place. */
static __declspec(naked) void ot_capture_input_hook(void) {
    __asm {
        pushad
        pushfd
        call ot_capture_input_boundary
        popfd
        popad
        jmp dword ptr [g_input_trampoline]
    }
}

/* The timer seam is a ten-byte pair of calls.  Observe the state immediately
 * before those calls and resume through the trampoline so the retail timer
 * model remains untouched. */
static __declspec(naked) void ot_capture_timer_hook(void) {
    __asm {
        pushad
        pushfd
        push OTCAP_TIMER_PHASE_TIMER_UPDATE
        call ot_capture_timer_boundary
        add esp, 4
        popfd
        popad
        jmp dword ptr [g_timer_trampoline]
    }
}

/* Sample the simulation clock load used by the timing producer. */
static __declspec(naked) void ot_capture_clock_read_hook(void) {
    __asm {
        pushad
        pushfd
        push OTCAP_TIMER_PHASE_CLOCK_READ
        call ot_capture_timer_boundary
        add esp, 4
        popfd
        popad
        jmp dword ptr [g_clock_read_trampoline]
    }
}
#endif

static int create_trampoline(
    unsigned char *target,
    const CaptureHookSpec *spec,
    uint8_t *original,
    void **trampoline_out) {
    unsigned char *trampoline;
    long relative;
    uint32_t size;

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
    trampoline[spec->overwrite_size] = 0xe9;
    relative = (long)(target + spec->overwrite_size -
        (trampoline + spec->overwrite_size + 5u));
    memcpy(trampoline + spec->overwrite_size + 1u, &relative, sizeof(relative));
    *trampoline_out = trampoline;
    return 1;
}

static int patch_target(
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
    relative = (long)((unsigned char *)hook -
        (target + 5u));
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

static int restore_target(
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

int ot_capture_install_hooks(void *module_base) {
    const CaptureHookSpec *physics;
    const CaptureHookSpec *action;
    const CaptureHookSpec *timer;
    const CaptureHookSpec *clock_read;
    unsigned char *physics_target;
    unsigned char *action_target;
    unsigned char *timer_target;
    unsigned char *clock_read_target;

    if (g_physics_installed || g_input_installed || g_timer_installed ||
        g_clock_read_installed ||
        g_capture_buffer == 0 ||
        g_capture_buffer->mapping == 0 ||
        (unsigned long)module_base != 0x00400000UL) {
        return 0;
    }
#if !defined(_MSC_VER) || !defined(_M_IX86)
    (void)module_base;
    return 0;
#else
    int physics_patched = 0;
    int action_patched = 0;
    int timer_patched = 0;
    int clock_read_patched = 0;
    physics = find_hook("physics_frame");
    action = find_hook("action_mask_injection");
    timer = find_hook("timer_update");
    clock_read = find_hook("clock_read");
    if (physics == 0 || action == 0 || physics->overwrite_size != 6u ||
        action->overwrite_size != 5u || timer == 0 ||
        timer->overwrite_size != 10u || clock_read == 0 ||
        clock_read->overwrite_size != 6u ||
        !ot_capture_verify_hooks(module_base, 0)) {
        return 0;
    }
    physics_target = (unsigned char *)module_base + physics->rva;
    action_target = (unsigned char *)module_base + action->rva;
    timer_target = (unsigned char *)module_base + timer->rva;
    clock_read_target = (unsigned char *)module_base + clock_read->rva;
    g_physics_target = physics_target;
    g_physics_overwrite_size = physics->overwrite_size;
    g_input_target = action_target;
    g_input_overwrite_size = action->overwrite_size;
    g_timer_target = timer_target;
    g_timer_overwrite_size = timer->overwrite_size;
    g_clock_read_target = clock_read_target;
    g_clock_read_overwrite_size = clock_read->overwrite_size;
    if (!create_trampoline(
            physics_target, physics, g_physics_original, &g_physics_trampoline)) {
        goto install_failed;
    }
    if (!create_trampoline(
            action_target, action, g_input_original, &g_input_trampoline)) {
        goto install_failed;
    }
    if (!create_trampoline(
            timer_target, timer, g_timer_original, &g_timer_trampoline)) {
        goto install_failed;
    }
    if (!create_trampoline(
            clock_read_target, clock_read, g_clock_read_original,
            &g_clock_read_trampoline)) {
        goto install_failed;
    }
    if (!patch_target(
            physics_target, physics->overwrite_size,
            (void *)ot_capture_physics_hook, g_physics_original)) {
        goto install_failed;
    }
    physics_patched = 1;
    if (!patch_target(
            action_target, action->overwrite_size,
            (void *)ot_capture_input_hook, g_input_original)) {
        goto install_failed;
    }
    action_patched = 1;
    if (!patch_target(
            timer_target, timer->overwrite_size,
            (void *)ot_capture_timer_hook, g_timer_original)) {
        goto install_failed;
    }
    timer_patched = 1;
    if (!patch_target(
            clock_read_target, clock_read->overwrite_size,
            (void *)ot_capture_clock_read_hook, g_clock_read_original)) {
        goto install_failed;
    }
    clock_read_patched = 1;
    g_physics_installed = 1;
    g_input_installed = 1;
    g_timer_installed = 1;
    g_clock_read_installed = 1;
    return 1;

install_failed:
        if (clock_read_patched) {
            restore_target(
                clock_read_target, clock_read->overwrite_size,
                g_clock_read_original);
        }
        if (timer_patched) {
            restore_target(
                timer_target, timer->overwrite_size, g_timer_original);
        }
        if (action_patched) {
            restore_target(
                action_target, action->overwrite_size, g_input_original);
        }
        if (physics_patched) {
            restore_target(
                physics_target, physics->overwrite_size,
                g_physics_original);
        }
        if (g_physics_trampoline != 0) {
            VirtualFree(g_physics_trampoline, 0, MEM_RELEASE);
        }
        if (g_input_trampoline != 0) {
            VirtualFree(g_input_trampoline, 0, MEM_RELEASE);
        }
        if (g_timer_trampoline != 0) {
            VirtualFree(g_timer_trampoline, 0, MEM_RELEASE);
        }
        if (g_clock_read_trampoline != 0) {
            VirtualFree(g_clock_read_trampoline, 0, MEM_RELEASE);
        }
        g_physics_trampoline = 0;
        g_input_trampoline = 0;
        g_timer_trampoline = 0;
        g_clock_read_trampoline = 0;
        g_physics_target = 0;
        g_input_target = 0;
        g_timer_target = 0;
        g_clock_read_target = 0;
        g_physics_overwrite_size = 0;
        g_input_overwrite_size = 0;
        g_timer_overwrite_size = 0;
        g_clock_read_overwrite_size = 0;
        return 0;
#endif
}
