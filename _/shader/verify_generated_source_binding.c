/* Execute the app's native source-binding boundary against actual generated
 * bytes. This host check does not submit a shader or qualify GPU arithmetic. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "holomorphic_shader_binding.h"
#include "runtime_source_binding.h"

#if HOLOMORPHIC_REQUIRE_GENERATED_SHADER != 1
#error "This verifier requires a generated-shader binding"
#endif

static unsigned char *read_exact_file(const char *path, size_t *length) {
    FILE *stream = fopen(path, "rb");
    if (stream == NULL || fseek(stream, 0, SEEK_END) != 0) {
        fprintf(stderr, "FAIL cannot read generated input: %s\n", path);
        exit(1);
    }
    long ending = ftell(stream);
    if (ending <= 0 || (uintmax_t)ending >= SIZE_MAX || fseek(stream, 0, SEEK_SET) != 0) {
        fprintf(stderr, "FAIL unusable generated input length: %s\n", path);
        exit(1);
    }
    *length = (size_t)ending;
    unsigned char *bytes = malloc(*length + 1);
    if (bytes == NULL || fread(bytes, 1, *length, stream) != *length || ferror(stream)) {
        fprintf(stderr, "FAIL incomplete generated input: %s\n", path);
        exit(1);
    }
    if (fclose(stream) != 0) {
        fprintf(stderr, "FAIL generated input close failed: %s\n", path);
        exit(1);
    }
    bytes[*length] = 0;
    return bytes;
}

int main(int argument_count, char **arguments) {
    if (argument_count != 3) {
        fputs("requires GENERATED_FRAGMENT GENERATED_PROVENANCE\n", stderr);
        return 2;
    }
    size_t source_length, receipt_length;
    unsigned char *source = read_exact_file(arguments[1], &source_length);
    unsigned char *receipt = read_exact_file(arguments[2], &receipt_length);
    if (memchr(receipt, 0, receipt_length) != NULL) {
        fputs("FAIL embedded provenance zero byte\n", stderr);
        free(source);
        free(receipt);
        return 1;
    }
    const struct holo_source_binding expected = {
        HOLOMORPHIC_FRAGMENT_LENGTH, HOLOMORPHIC_FRAGMENT_SHA256,
        HOLOMORPHIC_BACKEND_COMMIT, HOLOMORPHIC_APPLICATION_COMMIT
    };
    char actual_digest[65];
    const char *failure = holo_verify_source_binding((const char *)source, source_length,
        (const char *)receipt, &expected, actual_digest);
    free(source);
    free(receipt);
    if (failure != NULL) {
        fprintf(stderr, "FAIL actual generated source binding: %s\n", failure);
        return 1;
    }
    printf("PASS actual generated source binding host check bytes=%zu sha256=%s\n",
        source_length, actual_digest);
    return 0;
}
