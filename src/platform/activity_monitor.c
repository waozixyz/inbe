#include "activity_monitor.h"

#include "kry_activity_monitor.h"

void
activity_monitor_init(void)
{
    KryActivityMonitorInit();
}

int
activity_is_wayland(void)
{
    return KryActivityIsWayland();
}

int
activity_available(void)
{
    return KryActivityAvailable();
}

long
activity_get_idle_ms(void)
{
    return KryActivityGetIdleMilliseconds();
}

int
break_set_input_blocked(int on)
{
    return KryActivitySetInputBlocked(on);
}

int
break_input_blocked(void)
{
    return KryActivityInputBlocked();
}
