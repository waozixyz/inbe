#include "app.h"
#include "practices/session_results.h"
#include "practices/meditation/meditation_practice.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int write_ok, writes, syncs, switches, settings;
static int sessions;
int clampi(int value, int low, int high) { return value < low ? low : value > high ? high : value; }
void app_play_bell_cue(InnerBreeze*app, float volume) { (void)app; (void)volume; }
void app_close_modal(InnerBreeze*app) { (void)app; }
void meditation_music_start_session(InnerBreeze*app) { (void)app; }
void meditation_music_stop(InnerBreeze*app) { (void)app; }
void meditation_music_fade_out(InnerBreeze*app) { (void)app; }
void practice_background_start(InnerBreeze*app, int id) { (void)app; (void)id; }
void practice_active_background_stop(InnerBreeze*app) { (void)app; }
void app_notifications_send_session_complete(InnerBreeze*app) { (void)app; }
void sync_habits_for_activity(InnerBreeze*app, int id) { (void)app; (void)id; }
int data_save_session_path_for_activity(const int *rounds, int count, int topic, int activity, char *out, size_t size) {
    assert(rounds[0] == 60 && count == 1 && topic == 0 && activity == 1);
    sessions++;
    if(out && size) snprintf(out, size, "session-1");
    return 1;
}
void PushUIInspectSource(const char *path, int line) { (void)path; (void)line; }
void PopUIInspectSource(void) {}
int practice_count(void) { return 4; }
int practice_clamp_id(int id) { return id >= 0 && id < 4 ? id : 0; }
int practice_ordered_id(int index) { return index; }
void save_settings(InnerBreeze*app) { (void)app; settings++; }
int app_auto_sync(InnerBreeze*app) { (void)app; syncs++; return 1; }
void app_switch_screen(InnerBreeze*app, int screen) {
    app->breathing.screen = screen; switches++;
}
int data_discard_session(const char *path) {
    assert(strcmp(path, "session-1") == 0); writes++; return write_ok;
}
int data_save_session_checkin(const char *path, const StorageSessionCheckin *checkin) {
    assert(strcmp(path, "session-1") == 0);
    assert(checkin->mood_after == 4); writes++; return write_ok;
}
int main(void) {
    InnerBreeze app = {0};
    app_prepare_session_results(&app, 1, 60, 0, NULL, NULL, 0, "session-1");
    assert(settings == 0);
    app.session_result.mood = 4;
    session_result_done(&app);
    assert(app.session_result.active && app.session_result.write_failed);
    assert(app.session_result.mood == 4 && switches == 0 && syncs == 0);
    write_ok = 1;
    session_result_done(&app);
    assert(!app.session_result.active && switches == 1 && syncs == 1);
    session_result_done(&app);
    assert(writes == 2 && switches == 1);
    app_prepare_session_results(&app, 2, 3, 0, NULL, NULL, 0, "session-1");
    write_ok = 0;
    session_result_discard(&app);
    assert(app.session_result.active && app.session_result.write_failed);
    write_ok = 1;
    session_result_discard(&app);
    assert(!app.session_result.active && switches == 2 && syncs == 2);
    session_result_discard(&app);
    assert(writes == 4);
    app_prepare_session_results(&app, 0, 60, 0, NULL, NULL, 0, NULL);
    session_result_done(&app);
    assert(!app.session_result.active);
    app.meditation.duration_mode = 5;
    app.meditation.custom_minutes = 1;
    meditation_start_configured(&app);
    assert(app.meditation.remaining_seconds == 60);
    meditation_advance_elapsed(&app, 1500);
    assert(app.meditation.remaining_seconds == 59);
    app.session_paused = 1;
    meditation_advance_elapsed(&app, 30000);
    assert(app.meditation.remaining_seconds == 59);
    app.session_paused = 0;
    meditation_advance_elapsed(&app, 59000);
    assert(app.breathing.screen == ScreenResults && sessions == 1);
    assert(app.session_result.saved && app.session_result.primary_value == 60);
    meditation_advance_elapsed(&app, 60000);
    assert(sessions == 1);
    app.session_result.mood = 4;
    session_result_done(&app);
    assert(app.breathing.screen == ScreenStart && !app.session_result.active);
    puts("session results and practice preferences tests passed");
    return 0;
}
