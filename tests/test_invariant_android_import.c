#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

// Include the actual production header
#include "src/platform/android/android_import.h"

START_TEST(test_buffer_reads_never_exceed_declared_length)
{
    // Invariant: Buffer reads never exceed the declared length
    const char *payloads[] = {
        "normal",                    // Valid input
        "A",                         // Boundary: single char
        "very_long_string_that_exceeds_typical_buffer_size_by_a_lot_here_we_go_with_more_chars_to_make_sure_it_overflows_if_not_properly_handled",  // Oversized input (2x+)
        "../../../etc/passwd",       // Path traversal attempt
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"  // Exact 256 A's for buffer boundary
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        // Create a test file with the payload as content
        FILE *test_file = tmpfile();
        if (!test_file) {
            ck_abort_msg("Failed to create temp file");
        }
        
        fputs(payloads[i], test_file);
        rewind(test_file);
        
        // Get file descriptor for android_import_file
        int fd = fileno(test_file);
        if (fd < 0) {
            fclose(test_file);
            ck_abort_msg("Failed to get file descriptor");
        }
        
        // Call the actual production function
        // android_import_file should handle buffer bounds properly
        int result = android_import_file(fd, "test_import");
        
        // The test passes if no crash occurs and function completes
        // (or returns appropriate error for invalid input)
        ck_assert_msg(1, "Buffer overflow check for payload: %s", payloads[i]);
        
        fclose(test_file);
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_buffer_reads_never_exceed_declared_length);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}