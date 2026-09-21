#include "kryon.h"

const char *Plan9Run(void);

void
main(void)
{
    exits((char *)Plan9Run());
}
