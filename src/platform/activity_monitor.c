#include "activity_monitor.h"

#include "kry_activity_monitor.h"

void
activity_monitor_init(void)
{
    ActivityMonitorInit();
}

int
activity_is_wayland(void)
{
    return ActivityIsWayland();
}

int
activity_available(void)
{
    return ActivityAvailable();
}

long
activity_get_idle_ms(void)
{
    return ActivityGetIdleMilliseconds();
}

int
break_set_input_blocked(int on)
{
    return ActivitySetInputBlocked(on);
}

int
break_input_blocked(void)
{
    return ActivityInputBlocked();
}
