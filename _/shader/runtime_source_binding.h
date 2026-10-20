#ifndef HOLOMORPHIC_RUNTIME_SOURCE_BINDING_H
#define HOLOMORPHIC_RUNTIME_SOURCE_BINDING_H

/* Bind the bytes submitted to glShaderSource to a digest compiled into the
 * native library from the generated build input. A neighbouring asset is not
 * an independent trust anchor. This portable boundary is also host tested. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Inspect only the supplied readable span, excluding an appended terminator.
 * This explicit bounded scan also works with the native Fortify policy that
 * rejects unqualified memory-search library calls. */
static inline int holo_source_contains_nul(const void *source, size_t length) {
    const unsigned char *bytes = source;
    for (size_t index = 0; index < length; ++index) {
        if (bytes[index] == 0) return 1;
    }
    return 0;
}

static inline uint32_t holo_rotate_right(uint32_t value, unsigned int count) {
    return (value >> count) | (value << (32u - count));
}

static inline void holo_sha256_block(uint32_t state[8], const unsigned char block[64]) {
    static const uint32_t constants[64] = {
        0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
        0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
        0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
        0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
        0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
        0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
        0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
        0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
    };
    uint32_t words[64];
    for (size_t index = 0; index < 16; ++index) {
        words[index] = ((uint32_t)block[4 * index] << 24) |
            ((uint32_t)block[4 * index + 1] << 16) |
            ((uint32_t)block[4 * index + 2] << 8) | block[4 * index + 3];
    }
    for (size_t index = 16; index < 64; ++index) {
        uint32_t first = words[index - 15], second = words[index - 2];
        words[index] = words[index - 16] + words[index - 7] +
            (holo_rotate_right(first, 7) ^ holo_rotate_right(first, 18) ^ (first >> 3)) +
            (holo_rotate_right(second, 17) ^ holo_rotate_right(second, 19) ^ (second >> 10));
    }
    uint32_t a=state[0], b=state[1], c=state[2], d=state[3];
    uint32_t e=state[4], f=state[5], g=state[6], h=state[7];
    for (size_t index = 0; index < 64; ++index) {
        uint32_t first = h + (holo_rotate_right(e, 6) ^ holo_rotate_right(e, 11) ^ holo_rotate_right(e, 25)) +
            ((e & f) ^ (~e & g)) + constants[index] + words[index];
        uint32_t second = (holo_rotate_right(a, 2) ^ holo_rotate_right(a, 13) ^ holo_rotate_right(a, 22)) +
            ((a & b) ^ (a & c) ^ (b & c));
        h=g; g=f; f=e; e=d+first; d=c; c=b; b=a; a=first+second;
    }
    state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
    state[4]+=e; state[5]+=f; state[6]+=g; state[7]+=h;
}

static inline void holo_source_sha256(const void *source, size_t length, char output[65]) {
    static const char digits[] = "0123456789abcdef";
    uint32_t state[8] = {0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
        0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
    const unsigned char *bytes = source;
    size_t offset = 0;
    while (length - offset >= 64) {
        holo_sha256_block(state, bytes + offset);
        offset += 64;
    }
    unsigned char tail[128] = {0};
    size_t remaining = length - offset;
    if (remaining != 0) memcpy(tail, bytes + offset, remaining);
    tail[remaining] = 0x80;
    size_t padded = remaining < 56 ? 64 : 128;
    uint64_t bit_length = (uint64_t)length * 8u;
    for (size_t index = 0; index < 8; ++index)
        tail[padded - 1 - index] = (unsigned char)(bit_length >> (8 * index));
    holo_sha256_block(state, tail);
    if (padded == 128) holo_sha256_block(state, tail + 64);
    for (size_t index = 0; index < 32; ++index) {
        unsigned char byte = (unsigned char)(state[index / 4] >> (24 - 8 * (index % 4)));
        output[2 * index] = digits[byte >> 4];
        output[2 * index + 1] = digits[byte & 15];
    }
    output[64] = '\0';
}

/* Require one whole key=value line: duplicates, substrings and missing fields
 * cannot make an unrelated sidecar look like the selected compiler receipt. */
static inline int holo_receipt_field(const char *receipt, const char *key, const char *expected) {
    if (receipt == NULL) return 0;
    size_t key_length = strlen(key), expected_length = strlen(expected);
    int matches = 0;
    const char *line = receipt;
    while (*line != '\0') {
        const char *end = strchr(line, '\n');
        size_t length = end == NULL ? strlen(line) : (size_t)(end - line);
        if (length > key_length && memcmp(line, key, key_length) == 0 && line[key_length] == '=') {
            if (length != key_length + 1 + expected_length ||
                memcmp(line + key_length + 1, expected, expected_length) != 0) return 0;
            ++matches;
        }
        if (end == NULL) break;
        line = end + 1;
    }
    return matches == 1;
}

struct holo_source_binding {
    size_t length;
    const char *sha256;
    const char *backend_commit;
    const char *application_commit;
};

static inline const char *holo_verify_source_binding(
    const void *source, size_t length, const char *receipt,
    const struct holo_source_binding *expected, char actual_sha256[65]
) {
    if (source == NULL || expected == NULL || length == 0) return "missing-source";
    if (receipt == NULL) return "missing-provenance";
    if (length != expected->length) return "source-length-mismatch";
    if (holo_source_contains_nul(source, length)) return "embedded-source-nul";
    holo_source_sha256(source, length, actual_sha256);
    if (strcmp(actual_sha256, expected->sha256) != 0) return "source-digest-mismatch";
    if (!holo_receipt_field(receipt, "compiler", "idris2-glsles") ||
        !holo_receipt_field(receipt, "backend_repo", "fuego-ironworks/idris-shader-backend") ||
        !holo_receipt_field(receipt, "application_repo", "isomorphismes/holomorphic") ||
        !holo_receipt_field(receipt, "backend_commit", expected->backend_commit) ||
        !holo_receipt_field(receipt, "application_commit", expected->application_commit) ||
        !holo_receipt_field(receipt, "fragment_sha256", expected->sha256))
        return "provenance-identity-mismatch";
    return NULL;
}

#endif
