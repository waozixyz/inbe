#include "sync_client.h"

#include <stdio.h>
#include <string.h>

int sync_client_test_response_buffer(const char *first, const char *second,
                                          char *out, size_t out_size);

static int failures = 0;

static void
check_valid(const char *input, const char *normalized)
{
    char out[256];

    if(!sync_client_url_valid(input)) {
        fprintf(stderr, "FAIL valid %s\n", input);
        failures++;
        return;
    }
    if(!sync_client_normalize_url(input, out, sizeof(out)) ||
       strcmp(out, normalized) != 0) {
        fprintf(stderr, "FAIL normalize %s: got %s, want %s\n",
                input, out, normalized);
        failures++;
    }
}

static void
check_invalid(const char *input)
{
    char out[256];

    if(sync_client_url_valid(input)) {
        fprintf(stderr, "FAIL invalid accepted %s\n", input);
        failures++;
    }
    if(sync_client_normalize_url(input, out, sizeof(out))) {
        fprintf(stderr, "FAIL invalid normalized %s to %s\n", input, out);
        failures++;
    }
}

static void
check_response_buffer(void)
{
    char out[64];

    if(!sync_client_test_response_buffer("{\"nonce\":\"", "abc\"}", out, sizeof(out)) ||
       strcmp(out, "{\"nonce\":\"abc\"}") != 0) {
        fprintf(stderr, "FAIL response buffer append: got %s\n", out);
        failures++;
    }
}

int
main(void)
{
    check_valid("https://api.waozi.xyz", "https://api.waozi.xyz");
    check_valid("http://localhost:8080", "http://localhost:8080");
    check_valid("localhost:8080", "http://localhost:8080");
    check_valid("localhost:3000", "http://localhost:3000");
    check_valid("127.0.0.1:8080", "http://127.0.0.1:8080");
    check_valid("127.0.0.1:49152", "http://127.0.0.1:49152");
    check_valid("10.0.2.2:8080", "http://10.0.2.2:8080");

    check_invalid("");
    check_invalid("localhost.evil.test:8080");
    check_invalid("http://localhost.evil.test");
    check_invalid("http://example.com");

    check_response_buffer();

    if(failures != 0) {
        fprintf(stderr, "%d sync URL test failure(s)\n", failures);
        return 1;
    }
    printf("sync URL tests passed\n");
    return 0;
}
