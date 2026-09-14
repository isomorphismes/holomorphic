#include "shader_sha256.h"

#include <stdio.h>
#include <string.h>

static int expect_hash(
    const char *name,
    const char *input,
    const char *expected
) {
    char actual[SHADER_SHA256_HEX_LENGTH + 1];
    shader_sha256_hex(input, strlen(input), actual);
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected %s, got %s\n", name, expected, actual);
        return 1;
    }
    return 0;
}

int main(void) {
    int failures = 0;
    failures += expect_hash(
        "empty",
        "",
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
    );
    failures += expect_hash(
        "abc",
        "abc",
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
    );
    return failures == 0 ? 0 : 1;
}
