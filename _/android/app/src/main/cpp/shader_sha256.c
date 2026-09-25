#include "shader_sha256.h"

#include <stdint.h>
#include <string.h>

struct sha256_state {
    uint32_t words[8];
    uint64_t total_bytes;
    uint8_t block[64];
    size_t block_length;
};

static uint32_t rotate_right(uint32_t value, unsigned amount) {
    return (value >> amount) | (value << (32u - amount));
}

static uint32_t load_be32(const uint8_t bytes[4]) {
    return ((uint32_t)bytes[0] << 24) |
           ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) |
           (uint32_t)bytes[3];
}

static void store_be64(uint8_t bytes[8], uint64_t value) {
    for (int index = 7; index >= 0; --index) {
        bytes[index] = (uint8_t)(value & 0xffu);
        value >>= 8;
    }
}

static void transform(struct sha256_state *state, const uint8_t block[64]) {
    static const uint32_t constants[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
        0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
        0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
        0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
        0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
        0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
        0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
        0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
        0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
        0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
        0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
    };
    uint32_t schedule[64];
    for (int index = 0; index < 16; ++index) {
        schedule[index] = load_be32(block + 4 * index);
    }
    for (int index = 16; index < 64; ++index) {
        uint32_t first = schedule[index - 15];
        uint32_t second = schedule[index - 2];
        uint32_t sigma0 =
            rotate_right(first, 7) ^ rotate_right(first, 18) ^ (first >> 3);
        uint32_t sigma1 =
            rotate_right(second, 17) ^ rotate_right(second, 19) ^ (second >> 10);
        schedule[index] =
            schedule[index - 16] + sigma0 + schedule[index - 7] + sigma1;
    }

    uint32_t a = state->words[0];
    uint32_t b = state->words[1];
    uint32_t c = state->words[2];
    uint32_t d = state->words[3];
    uint32_t e = state->words[4];
    uint32_t f = state->words[5];
    uint32_t g = state->words[6];
    uint32_t h = state->words[7];

    for (int index = 0; index < 64; ++index) {
        uint32_t sum1 =
            rotate_right(e, 6) ^ rotate_right(e, 11) ^ rotate_right(e, 25);
        uint32_t choose = (e & f) ^ ((~e) & g);
        uint32_t temp1 =
            h + sum1 + choose + constants[index] + schedule[index];
        uint32_t sum0 =
            rotate_right(a, 2) ^ rotate_right(a, 13) ^ rotate_right(a, 22);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = sum0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state->words[0] += a;
    state->words[1] += b;
    state->words[2] += c;
    state->words[3] += d;
    state->words[4] += e;
    state->words[5] += f;
    state->words[6] += g;
    state->words[7] += h;
}

static void initialize(struct sha256_state *state) {
    static const uint32_t initial[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
    };
    memcpy(state->words, initial, sizeof(initial));
    state->total_bytes = 0;
    state->block_length = 0;
}

static void update(
    struct sha256_state *state,
    const uint8_t *data,
    size_t length
) {
    state->total_bytes += length;
    while (length > 0) {
        size_t space = sizeof(state->block) - state->block_length;
        size_t amount = length < space ? length : space;
        memcpy(state->block + state->block_length, data, amount);
        state->block_length += amount;
        data += amount;
        length -= amount;
        if (state->block_length == sizeof(state->block)) {
            transform(state, state->block);
            state->block_length = 0;
        }
    }
}

static void finish(struct sha256_state *state, uint8_t digest[32]) {
    uint64_t bit_length = state->total_bytes * 8u;
    state->block[state->block_length++] = 0x80u;
    if (state->block_length > 56) {
        memset(
            state->block + state->block_length,
            0,
            sizeof(state->block) - state->block_length
        );
        transform(state, state->block);
        state->block_length = 0;
    }
    memset(state->block + state->block_length, 0, 56 - state->block_length);
    store_be64(state->block + 56, bit_length);
    transform(state, state->block);

    for (int index = 0; index < 8; ++index) {
        digest[4 * index] = (uint8_t)(state->words[index] >> 24);
        digest[4 * index + 1] = (uint8_t)(state->words[index] >> 16);
        digest[4 * index + 2] = (uint8_t)(state->words[index] >> 8);
        digest[4 * index + 3] = (uint8_t)state->words[index];
    }
}

void shader_sha256_hex(
    const void *data,
    size_t length,
    char output[SHADER_SHA256_HEX_LENGTH + 1]
) {
    static const char hexadecimal[] = "0123456789abcdef";
    struct sha256_state state;
    uint8_t digest[32];
    initialize(&state);
    update(&state, (const uint8_t *)data, length);
    finish(&state, digest);
    for (int index = 0; index < 32; ++index) {
        output[2 * index] = hexadecimal[digest[index] >> 4];
        output[2 * index + 1] = hexadecimal[digest[index] & 0x0fu];
    }
    output[SHADER_SHA256_HEX_LENGTH] = '\0';
}
