#include "src/practices/meditation/meditation_music.h"

void
meditation_music_format_download_status(char *out, size_t out_size,
                                        const RuntimeAssetDownload *download)
{
    (void)download;
    if(out != 0 && out_size > 0)
        out[0] = '\0';
}

void
meditation_music_draw_download_progress(InbeApp *app, int x, int y, int w)
{
    (void)app;
    (void)x;
    (void)y;
    (void)w;
}

int
meditation_music_available(InbeApp *app)
{
    (void)app;
    return 0;
}

const char *
meditation_music_selected_label(InbeApp *app)
{
    (void)app;
    return "None";
}

void meditation_music_init(InbeApp *app) { (void)app; }
void meditation_music_unload(InbeApp *app) { (void)app; }
void meditation_music_stop(InbeApp *app) { (void)app; }
void meditation_music_fade_out(InbeApp *app) { (void)app; }
void meditation_music_start_session(InbeApp *app) { (void)app; }
void meditation_music_update(InbeApp *app) { (void)app; }
void meditation_music_start_download(InbeApp *app) { (void)app; }

void
meditation_music_draw_practice_settings(InbeApp *app, int practice,
                                        int content_x, int content_w, int *y,
                                        int show_installed_download,
                                        int show_status)
{
    (void)app;
    (void)practice;
    (void)content_x;
    (void)content_w;
    (void)y;
    (void)show_installed_download;
    (void)show_status;
}

int
meditation_music_measure_practice_settings(InbeApp *app, int practice,
                                           int content_w,
                                           int show_installed_download,
                                           int show_status)
{
    (void)app;
    (void)practice;
    (void)content_w;
    (void)show_installed_download;
    (void)show_status;
    return 0;
}
