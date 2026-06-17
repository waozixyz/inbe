#include "flint_scaling.h"
#include "flint_text.h"

#include <stdio.h>

static int g_failures = 0;

static void
check_int(const char *label, int got, int want)
{
    if(got == want)
        return;
    fprintf(stderr, "FAIL %s: got %d, want %d\n", label, got, want);
    g_failures++;
}

int
main(void)
{
    flint_set_dpi_scale(1.0f);
    check_int("1x 8px", flint_text_size(FLINT_TEXT_8), 8);
    check_int("1x 16px", flint_text_size(FLINT_TEXT_16), 16);

    flint_set_dpi_scale(2.0f);
    check_int("2x unscaled min resolves to scaled 8px",
              flint_text_size(FLINT_TEXT_8), 16);
    check_int("2x scaled 16px stays scaled",
              flint_text_size(flint_px(FLINT_TEXT_16)), 32);

    flint_set_dpi_scale(3.0f);
    check_int("3x unscaled min resolves to scaled 8px",
              flint_text_size(FLINT_TEXT_8), 24);
    check_int("3x scaled 16px stays scaled",
              flint_text_size(flint_px(FLINT_TEXT_16)), 48);

    return g_failures == 0 ? 0 : 1;
}
