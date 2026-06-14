#include "flint_color.h"

Color
flint_lighten(Color c, int amount)
{
    return (Color){
        (unsigned char)(c.r + amount > 255 ? 255 : c.r + amount),
        (unsigned char)(c.g + amount > 255 ? 255 : c.g + amount),
        (unsigned char)(c.b + amount > 255 ? 255 : c.b + amount),
        c.a
    };
}

Color
flint_darken(Color c, int amount)
{
    return (Color){
        (unsigned char)(c.r < amount ? 0 : c.r - amount),
        (unsigned char)(c.g < amount ? 0 : c.g - amount),
        (unsigned char)(c.b < amount ? 0 : c.b - amount),
        c.a
    };
}
