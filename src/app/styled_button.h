#ifndef APP_STYLED_BUTTON_H
#define APP_STYLED_BUTTON_H

/* Inbe-side stand-in for kryon's removed StyledButton/ButtonStyle API,
 * lowered onto the tone/emphasis control styling that replaced it. */

typedef enum ButtonStyle {
    ButtonStylePrimary,
    ButtonStyleSecondary,
    ButtonStyleOutline,
    ButtonStyleDanger,
    ButtonStyleTab,
    ButtonStyleTabSelected
} ButtonStyle;

int StyledButton(int x, int y, int w, int h, const char *label,
                 ButtonStyle style, int disabled, int *hover);

#endif
