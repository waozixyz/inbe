#include "kry_activity_monitor.h"

#include <stdio.h>

/*
 * Activity monitor smoke test.
 *
 * The monitor is platform-dependent: on a real X session it must report
 * availability and a sane idle counter; headless or Wayland-native sessions
 * must degrade to "unavailable" (idle -1, grabs unsupported) without
 * crashing. Everything is asserted for both outcomes; a debug line prints
 * which path this machine took.
 */

static int failures = 0;

static void
expect(int condition, const char *message)
{
    if(!condition) {
        fprintf(stderr, "FAIL %s\n", message);
        failures++;
    }
}

static void
test_init_is_idempotent(void)
{
    ActivityMonitorInit();
    ActivityMonitorInit();
    expect(1, "init runs twice without crashing");
}

static void
test_idle_matches_availability(void)
{
    int available = ActivityAvailable();
    long idle = ActivityGetIdleMilliseconds();

    printf("activity monitor: %s%s\n",
           available ? "system-wide (X idle counter)" : "unavailable",
           ActivityIsWayland() ? " (Wayland session)" : "");

    if(available) {
        expect(idle >= 0, "available monitor reports a non-negative idle");
        /* No input occurs between the two reads, so idle cannot go down. */
        {
            long again = ActivityGetIdleMilliseconds();

            expect(again >= idle, "idle does not decrease between reads");
        }
    } else {
        expect(idle == -1, "unavailable monitor reports idle -1");
    }
}

static void
test_input_grab_state(void)
{
    int supported = ActivitySetInputBlocked(1);

    if(supported) {
        expect(ActivityInputBlocked(), "grab reports held after request");
        expect(ActivitySetInputBlocked(1), "re-grabbing is a no-op success");
        expect(ActivitySetInputBlocked(0), "ungrab succeeds");
        expect(!ActivityInputBlocked(), "grab reports released");
    } else {
        expect(!ActivityInputBlocked(),
               "unsupported grab never reports held");
        expect(ActivitySetInputBlocked(0) == 0 ||
               ActivitySetInputBlocked(0) == 1,
               "ungrab without grab does not crash");
    }
}

int
main(void)
{
    test_init_is_idempotent();
    test_idle_matches_availability();
    test_input_grab_state();

    if(failures) {
        printf("FAIL (%d assertion(s))\n", failures);
        return 1;
    }
    printf("OK\n");
    return 0;
}
