#include "runtime_source_binding.h"
#include <stdio.h>
#include <stdlib.h>

static void require(int condition, const char *label) {
    if (!condition) {
        fprintf(stderr, "FAIL %s\n", label);
        exit(1);
    }
    printf("PASS %s\n", label);
}

int main(void) {
    const unsigned char text_span[] = {'a', 'b', 'c', 0};
    require(!holo_source_contains_nul(NULL, 0), "empty-span-needs-no-readable-byte");
    require(!holo_source_contains_nul(text_span, 3), "terminator-outside-source-span-ignored");
    require(holo_source_contains_nul(text_span, sizeof(text_span)), "nul-at-final-span-byte-found");
    require(holo_source_contains_nul(text_span + 3, 1), "nul-at-first-span-byte-found");
    char digest[65];
    holo_source_sha256("", 0, digest);
    require(strcmp(digest, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == 0, "sha256-empty-vector");
    holo_source_sha256("abc", 3, digest);
    require(strcmp(digest, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0, "sha256-abc-vector");
    const char long_vector[] = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    holo_source_sha256(long_vector, strlen(long_vector), digest);
    require(strcmp(digest, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1") == 0, "sha256-two-block-padding-vector");
    char million[1000000];
    memset(million, 'a', sizeof(million));
    holo_source_sha256(million, sizeof(million), digest);
    require(strcmp(digest, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0") == 0, "sha256-million-byte-vector");

    /* These are contract fixtures, not compiler or GPU acceptance evidence. */
    const struct holo_source_binding expected = {
        3, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "f32f27a37a176a7b2c580aa703c5d33df9511728",
        "3cfcf71a2949e453c679a99b2cc165e7dfbf425f"
    };
    const char receipt[] =
        "compiler=idris2-glsles\nbackend_repo=fuego-ironworks/idris-shader-backend\n"
        "backend_commit=f32f27a37a176a7b2c580aa703c5d33df9511728\n"
        "application_repo=isomorphismes/holomorphic\n"
        "application_commit=3cfcf71a2949e453c679a99b2cc165e7dfbf425f\n"
        "fragment_sha256=ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\n";
    require(holo_verify_source_binding("abc", 3, receipt, &expected, digest) == NULL, "exact-source-and-receipt-accepted");
    require(strcmp(holo_verify_source_binding("abd", 3, receipt, &expected, digest), "source-digest-mismatch") == 0,
        "swapped-source-unchanged-sidecar-rejected");
    require(strcmp(holo_verify_source_binding("abc", 3, NULL, &expected, digest), "missing-provenance") == 0,
        "missing-provenance-rejected");
    require(strcmp(holo_verify_source_binding("abc\n", 4, receipt, &expected, digest), "source-length-mismatch") == 0,
        "changed-byte-length-rejected");
    require(strcmp(holo_verify_source_binding("a\0c", 3, receipt, &expected, digest), "embedded-source-nul") == 0,
        "gl-string-truncation-rejected");
    char changed_receipt[sizeof(receipt) + 80];
    memcpy(changed_receipt, receipt, sizeof(receipt));
    memcpy(changed_receipt + strlen("compiler="), "handwrittenxx", strlen("idris2-glsles"));
    require(strcmp(holo_verify_source_binding("abc", 3, changed_receipt, &expected, digest), "provenance-identity-mismatch") == 0,
        "handwritten-identity-rejected");
    snprintf(changed_receipt, sizeof(changed_receipt), "%scompiler=idris2-glsles\n", receipt);
    require(strcmp(holo_verify_source_binding("abc", 3, changed_receipt, &expected, digest), "provenance-identity-mismatch") == 0,
        "duplicate-provenance-field-rejected");
    return 0;
}
