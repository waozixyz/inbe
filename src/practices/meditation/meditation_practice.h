#ifndef INBE_MEDITATION_PRACTICE_H
#define INBE_MEDITATION_PRACTICE_H

#include "app_fwd.h"

/* Forward declaration; full definition in runtime_assets.h (via app.h). */
struct RuntimeAssetDownload;

void meditation_practice_init(InbeApp *app);
void meditation_practice_destroy(InbeApp *app);
void meditation_practice_start(InbeApp *app);
void meditation_practice_update(InbeApp *app);
void meditation_practice_leave_config(InbeApp *app);
void meditation_music_unload(InbeApp *app);
void meditation_music_update(InbeApp *app);
void meditation_music_start_download(InbeApp *app);
void meditation_music_draw_download_progress(InbeApp *app, int x, int y, int w);
void meditation_music_format_download_status(char *out, size_t out_size,
                                             const struct RuntimeAssetDownload *download);
int meditation_music_available(InbeApp *app);
int meditation_configured_duration_seconds(const InbeApp *app);
void meditation_start_configured(InbeApp *app);
void meditation_manual_draw(InbeApp *app);
void meditation_manual_close(InbeApp *app, int mark_seen);
void meditation_config_screen_draw(InbeApp *app);
void meditation_draw_setup_modal(InbeApp *app);
void meditation_draw_screen(InbeApp *app, int center_x, int center_y);
void meditation_request_exit(InbeApp *app);
void meditation_advance_elapsed(InbeApp *app, int elapsed_ms);

#endif
