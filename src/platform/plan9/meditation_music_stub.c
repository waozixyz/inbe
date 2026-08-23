#include "src/practices/meditation/meditation_music.h"

static void
set_status(InbeApp *app, const char *message)
{
    if(app == 0)
        return;
    snprintf(app->meditation.music_status,
             sizeof(app->meditation.music_status),
             "%s", message != 0 ? message : "");
}

static int
track_available(InbeApp *app, int track)
{
    char path[FS_PATH_MAX];

    return app_audio_music_path(app, track, path, sizeof(path)) &&
           app_audio_music_file_valid(path);
}

static int
load_track(InbeApp *app, int track)
{
    char path[FS_PATH_MAX];

    if(app == 0)
        return 0;
    app_audio_ensure_ready(app);
    if(!app->audio_ready) {
        TraceLog(LOG_ERROR,
                 "AUDIO: Cannot load meditation music because audio device is not ready");
        set_status(app, GetLocaleText("meditation_music_device_not_ready"));
        return 0;
    }

    if(track < 0 || track >= app_audio_music_count(app))
        track = 0;

    meditation_music_unload(app);
    if(!app_audio_music_path(app, track, path, sizeof(path)) ||
       !app_audio_music_file_valid(path)) {
        set_status(app, GetLocaleText("meditation_music_track_not_installed"));
        return 0;
    }

    app->meditation.music = LoadMusicStream(path);
    if(!IsMusicValid(app->meditation.music)) {
        TraceLog(LOG_ERROR, "AUDIO: Could not load meditation track: %s", path);
        set_status(app, GetLocaleText("meditation_music_track_load_failed"));
        app->meditation.music_loaded = 0;
        return 0;
    }

    app->meditation.music_loaded = 1;
    app->meditation.music_track = track;
    app->meditation.music_fade_out_ticks = 0;
    app->meditation.music_fade_out_total_ticks = 0;
    SetMusicVolume(app->meditation.music, (float)app->music_volume / 100.0f);
    TraceLog(LOG_INFO, "AUDIO: Loaded meditation track: %s", path);
    return 1;
}

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
    int i;

    if(app == 0)
        return 0;
    for(i = 0; i < MEDITATION_MUSIC_TRACK_COUNT; i++) {
        if(!track_available(app, i))
            return 0;
    }
    return 1;
}

const char *
meditation_music_selected_label(InbeApp *app)
{
    int track;

    if(app == 0)
        return "";
    track = app->meditation.music_track;
    if(track < 0 || track >= app_audio_music_count(app))
        track = 0;
    return app_audio_music_label(app, track);
}

void
meditation_music_init(InbeApp *app)
{
    if(app == 0)
        return;
    app_audio_music_sanitize_selection(app);
    if(app->meditation.music_status[0] == '\0') {
        if(meditation_music_available(app))
            set_status(app, GetLocaleText("meditation_music_installed"));
        else
            set_status(app, GetLocaleText("meditation_music_not_installed"));
    }
}

void
meditation_music_unload(InbeApp *app)
{
    if(app == 0)
        return;
    if(app->meditation.music_loaded) {
        StopMusicStream(app->meditation.music);
        UnloadMusicStream(app->meditation.music);
        app->meditation.music_loaded = 0;
    }
    app->meditation.music_playing = 0;
    app->meditation.music_test_playing = 0;
    app->meditation.music_fade_out_ticks = 0;
    app->meditation.music_fade_out_total_ticks = 0;
}

void
meditation_music_stop(InbeApp *app)
{
    if(app == 0)
        return;
    if(app->meditation.music_loaded)
        StopMusicStream(app->meditation.music);
    app->meditation.music_playing = 0;
    app->meditation.music_test_playing = 0;
    app->meditation.music_fade_out_ticks = 0;
    app->meditation.music_fade_out_total_ticks = 0;
}

void
meditation_music_fade_out(InbeApp *app)
{
    int fade_ticks;

    fade_ticks = 180;
    if(app == 0 || !app->meditation.music_loaded ||
       !app->meditation.music_playing)
        return;
    if(app->meditation.music_fade_out_ticks > 0)
        return;
    app->meditation.music_fade_out_ticks = fade_ticks;
    app->meditation.music_fade_out_total_ticks = fade_ticks;
}

void
meditation_music_start_session(InbeApp *app)
{
    int practice;
    int track;

    if(app == 0)
        return;
    app_audio_music_sanitize_selection(app);
    practice = app->exercise_type;
    if(practice < 0 || practice >= EXERCISE_COUNT)
        practice = EXERCISE_MEDITATION;
    track = app->meditation.music_practice_tracks[practice];
    if(track == INBE_AUDIO_MUSIC_NONE)
        return;
    if(!load_track(app, track))
        return;
    PlayMusicStream(app->meditation.music);
    app->meditation.music_playing = 1;
    app->meditation.music_test_playing = 0;
}

void
meditation_music_update(InbeApp *app)
{
    float volume;

    if(app == 0 || !app->meditation.music_loaded)
        return;
    volume = (float)app->music_volume / 100.0f;
    if(app->meditation.music_fade_out_ticks > 0 &&
       app->meditation.music_fade_out_total_ticks > 0) {
        volume *= (float)app->meditation.music_fade_out_ticks /
                  (float)app->meditation.music_fade_out_total_ticks;
        app->meditation.music_fade_out_ticks--;
        if(app->meditation.music_fade_out_ticks <= 0) {
            meditation_music_stop(app);
            return;
        }
    }
    SetMusicVolume(app->meditation.music, volume);
    UpdateMusicStream(app->meditation.music);
}

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
