#include "hooks.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string.h>

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
        0x34, 0xa8, 0xfb, 0xed, 0x52, 0xea, 0xac, 0x30,
        0x1e, 0xf0, 0x3f, 0x1a, 0xf2, 0x46, 0xc6, 0xf7,
        0x0a, 0xb3, 0x29, 0x52, 0x78, 0x78, 0x05, 0x6d,
        0x08, 0xf1, 0x6d, 0xef, 0x3e, 0xda, 0x07, 0xc3,
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

int ot_capture_install_hooks(void *module_base) {
    (void)module_base;
    /* M1 deliberately has no detours. */
    return 1;
}
