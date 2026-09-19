#include "app.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static uint32_t inbe_color_to_int(Color color) {
    return (uint32_t)ColorToInt(color);
}

static Color inbe_lightfield_color(int style_index, Color color, float alpha) {
    return style_index == 3 ? Fade(color, alpha) : color;
}

int app_register_style_pack_variant(const char *source, const char *label,
                                    ThemeColors colors, int style_index) {
    StyleColorToken tokens[] = {
        {"canvas", inbe_color_to_int(colors.background)},
        {"text", inbe_color_to_int(colors.text)},
        {"muted", inbe_color_to_int(colors.text_muted)},
        {"surface", inbe_color_to_int(inbe_lightfield_color(style_index, colors.surface, 0.87f))},
        {"surface-end", inbe_color_to_int(inbe_lightfield_color(style_index, colors.surface_raised, 0.93f))},
        {"card", inbe_color_to_int(inbe_lightfield_color(style_index, colors.surface_raised, 0.87f))},
        {"card-end", inbe_color_to_int(inbe_lightfield_color(style_index, colors.surface, 0.93f))},
        {"panel", inbe_color_to_int(colors.surface_raised)},
        {"panel-hover", inbe_color_to_int(colors.surface)},
        {"panel-pressed", inbe_color_to_int(colors.surface_sunken)},
        {"button", inbe_color_to_int(inbe_lightfield_color(style_index, colors.surface_raised, 0.87f))},
        {"button-end", inbe_color_to_int(inbe_lightfield_color(style_index, colors.surface, 0.93f))},
        {"button-hover", inbe_color_to_int(inbe_lightfield_color(style_index, colors.surface, 0.93f))},
        {"button-hover-end", inbe_color_to_int(inbe_lightfield_color(style_index, colors.surface_raised, 0.93f))},
        {"button-pressed", inbe_color_to_int(inbe_lightfield_color(style_index, colors.surface_sunken, 0.93f))},
        {"button-pressed-end", inbe_color_to_int(inbe_lightfield_color(style_index, colors.surface, 0.93f))},
        {"button-disabled", inbe_color_to_int(colors.surface)},
        {"dropdown-end", inbe_color_to_int(inbe_lightfield_color(style_index, colors.surface_raised, 0.93f))},
        {"border", inbe_color_to_int(colors.border)},
        {"border-soft", inbe_color_to_int(colors.divider)},
        {"border-hover", inbe_color_to_int(colors.border_strong)},
        {"border-hover-strong", inbe_color_to_int(colors.border_strong)},
        {"border-disabled", inbe_color_to_int(colors.divider)},
        {"card-border", inbe_color_to_int(colors.border)},
        {"outline-border", inbe_color_to_int(colors.border_strong)},
        {"focus", inbe_color_to_int(colors.focus)},
        {"accent", inbe_color_to_int(colors.accent)},
        {"accent-end", inbe_color_to_int(colors.accent_pressed)},
        {"accent-hover", inbe_color_to_int(colors.accent_hover)},
        {"accent-hover-end", inbe_color_to_int(colors.accent)},
        {"accent-border", inbe_color_to_int(colors.accent_pressed)},
        {"accent-ink", inbe_color_to_int(colors.on_accent)},
        {"danger", inbe_color_to_int(colors.danger)},
        {"danger-hover", inbe_color_to_int(colors.danger)},
        {"danger-ink", inbe_color_to_int(colors.on_danger)},
        {"danger-border", inbe_color_to_int(colors.danger)},
        {"row", inbe_color_to_int(inbe_lightfield_color(style_index, colors.surface, 0.67f))},
        {"thumb-ink", inbe_color_to_int(colors.on_accent)},
        {"divider", inbe_color_to_int(colors.divider)},
        {"track", inbe_color_to_int(colors.surface_sunken)},
        {"track-hover", inbe_color_to_int(colors.surface_raised)},
        {"field", inbe_color_to_int(colors.surface_sunken)},
        {"field-border", inbe_color_to_int(colors.border)},
        {"scroll-thumb", inbe_color_to_int(colors.text_muted)},
        {"scroll-border", inbe_color_to_int(colors.border_strong)},
    };
    return RegisterStylePackVariant("inbe", source, label, tokens,
                                    (int)(sizeof(tokens) / sizeof(tokens[0])));
}
