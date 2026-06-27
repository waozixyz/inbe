#ifndef INBE_SETTINGS_THEME_H
#define INBE_SETTINGS_THEME_H

#include "app_fwd.h"

typedef struct SettingsThemeState {
    int draw_theme_mode_menu;
    int draw_nav_mode_menu;
    int draw_transition_menu;
    int theme_picker_modal_open;
    int theme_picker_scroll;
} SettingsThemeState;

int settings_theme_content_height(int content_w);
void settings_theme_draw(InbeApp *app, int x, int w, int *y, SettingsThemeState *state);
void settings_theme_handle_overlays(InbeApp *app, SettingsThemeState *state);
void settings_screen_draw_theme_picker_modal(InbeApp *app);

#endif
