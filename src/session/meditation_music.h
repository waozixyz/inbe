#ifndef INBE_MEDITATION_MUSIC_H
#define INBE_MEDITATION_MUSIC_H

#include "app_fwd.h"

enum {
    MEDITATION_MUSIC_TRACK_COUNT = 3
};

void meditation_music_init(InbeApp *app);
void meditation_music_update(InbeApp *app);
void meditation_music_unload(InbeApp *app);
void meditation_music_start_session(InbeApp *app);
void meditation_music_stop(InbeApp *app);
int meditation_music_measure_settings(InbeApp *app, int content_w,
                                      int show_installed_download, int show_status);
void meditation_music_draw_settings(InbeApp *app, int content_x, int content_w, int *y);
void meditation_music_draw_guide_settings(InbeApp *app, int content_x, int content_w, int *y);
int meditation_music_draw_dropdown_menu(InbeApp *app);
int meditation_music_available(InbeApp *app);
const char *meditation_music_selected_label(InbeApp *app);

#endif
