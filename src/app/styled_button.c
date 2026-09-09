#include "styled_button.h"

#include "kryon.h"

int
StyledButton(int x, int y, int w, int h, const char *label,
             ButtonStyle style, int disabled, int *hover)
{
    ButtonProps button = {0};

    button.bounds = (Rectangle){(float)x, (float)y, (float)w, (float)h};
    button.label = label;
    button.font = GetFontSize();
    button.disabled = disabled;
    switch(style) {
    case ButtonStyleDanger:
        button.tone = ButtonToneDanger;
        button.emphasis = ButtonEmphasisFilled;
        break;
    case ButtonStyleOutline:
        button.tone = ButtonToneNeutral;
        button.emphasis = ButtonEmphasisOutline;
        break;
    case ButtonStyleTab:
        button.tone = ButtonToneNeutral;
        button.emphasis = ButtonEmphasisGhost;
        break;
    case ButtonStyleTabSelected:
        button.tone = ButtonToneAccent;
        button.emphasis = ButtonEmphasisSoft;
        button.selected = 1;
        break;
    case ButtonStyleSecondary:
        button.tone = ButtonToneNeutral;
        button.emphasis = ButtonEmphasisSoft;
        break;
    case ButtonStylePrimary:
    default:
        button.tone = ButtonToneAccent;
        button.emphasis = ButtonEmphasisFilled;
        break;
    }
    if(hover != NULL)
        *hover = 0;
    return Button(button);
}
