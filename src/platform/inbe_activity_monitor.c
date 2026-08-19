#include "inbe_activity_monitor.h"

#include "kry_activity_monitor.h"

void
inbe_activity_monitor_init(void)
{
    KryActivityMonitorInit();
}

int
inbe_activity_is_wayland(void)
{
    return KryActivityIsWayland();
}

int
inbe_activity_available(void)
{
    return KryActivityAvailable();
}

long
inbe_activity_get_idle_ms(void)
{
    return KryActivityGetIdleMilliseconds();
}

int
inbe_break_set_input_blocked(int on)
{
    return KryActivitySetInputBlocked(on);
}

int
inbe_break_input_blocked(void)
{
    return KryActivityInputBlocked();
}
