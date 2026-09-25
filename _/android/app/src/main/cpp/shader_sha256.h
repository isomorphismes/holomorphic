#ifndef HOLOMORPHIC_SHADER_SHA256_H
#define HOLOMORPHIC_SHADER_SHA256_H

#include <stddef.h>

#define SHADER_SHA256_HEX_LENGTH 64

void shader_sha256_hex(
    const void *data,
    size_t length,
    char output[SHADER_SHA256_HEX_LENGTH + 1]
);

#endif
