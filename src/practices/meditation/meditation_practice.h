#ifndef APP_MEDITATION_PRACTICE_H
#define APP_MEDITATION_PRACTICE_H

#include "app_fwd.h"

/* Forward declaration; full definition in runtime_assets.h (via app.h). */
struct RuntimeAssetDownload;

void meditation_practice_init(InnerBreeze*app);
void meditation_practice_destroy(InnerBreeze*app);
void meditation_practice_start(InnerBreeze*app);
void meditation_practice_update(InnerBreeze*app);
void meditation_practice_leave_config(InnerBreeze*app);
void meditation_music_unload(InnerBreeze*app);
void meditation_music_update(InnerBreeze*app);
void meditation_music_start_download(InnerBreeze*app);
void meditation_music_draw_download_progress(InnerBreeze*app, int x, int y, int w);
void meditation_music_format_download_status(char *out, size_t out_size,
                                             const struct RuntimeAssetDownload *download);
int meditation_music_available(InnerBreeze*app);
int meditation_configured_duration_seconds(const InnerBreeze*app);
void meditation_start_configured(InnerBreeze*app);
void meditation_manual_draw(InnerBreeze*app);
void meditation_manual_close(InnerBreeze*app, int mark_seen);
void meditation_config_screen_draw(InnerBreeze*app);
void meditation_draw_setup_modal(InnerBreeze*app);
void meditation_draw_screen(InnerBreeze*app, int center_x, int center_y);
void meditation_request_exit(InnerBreeze*app);
void meditation_advance_elapsed(InnerBreeze*app, int elapsed_ms);

#endif
