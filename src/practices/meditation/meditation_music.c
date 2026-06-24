#include "meditation_music.h"

#include "app.h"
#include "flint_locale.h"
#include "flint_theme.h"
#include "miniz.h"
#include "flint_color.h"
#include "flint_runtime_assets.h"
#include "flint_text.h"
#include "flint_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef INBE_MEDITATION_AUDIO_URL
#if defined(PLATFORM_WEB)
#define INBE_MEDITATION_AUDIO_URL "/web-assets/dl/inbe-meditation-audio-v1.zip"
#else
#define INBE_MEDITATION_AUDIO_URL "https://inbe.waozi.xyz/web-assets/dl/inbe-meditation-audio-v1.zip"
#endif
#endif

typedef struct MeditationTrack {
    const char *title;
    const char *file;
} MeditationTrack;

static const MeditationTrack g_tracks[MEDITATION_MUSIC_TRACK_COUNT] = {
    {"Deep Meditation", "Elijah_K/deep-meditation.ogg"},
    {"Path Of Meditation", "Elijah_K/path-of-meditation.ogg"},
    {"Truth Of Silence", "Elijah_K/truth-of-silence.ogg"}
};

static const char *g_track_options[MEDITATION_MUSIC_TRACK_COUNT] = {
    "Deep Meditation",
    "Path Of Meditation",
    "Truth Of Silence"
};

static int load_track(InbeApp *app, int track);

static int
file_exists(const char *path)
{
    FILE *file;
    if(path == NULL || path[0] == '\0')
        return 0;
    file = fopen(path, "rb");
    if(file == NULL)
        return 0;
    fclose(file);
    return 1;
}

static int
safe_archive_member(const char *path)
{
    if(path == NULL || path[0] == '\0')
        return 0;
    if(path[0] == '/' || strstr(path, "..") != NULL)
        return 0;
    return 1;
}

static int
known_track_archive_member(const char *path)
{
    if(path == NULL)
        return 0;

    for(int i = 0; i < MEDITATION_MUSIC_TRACK_COUNT; i++) {
        if(strcmp(path, g_tracks[i].file) == 0)
            return 1;
    }

    return 0;
}

static int
copy_text_checked(char *out, size_t out_size, const char *text)
{
    size_t len;

    if(out == NULL || out_size == 0)
        return 0;
    if(text == NULL)
        text = "";

    len = strlen(text);
    if(len >= out_size) {
        out[0] = '\0';
        return 0;
    }

    memcpy(out, text, len + 1);
    return 1;
}

static int
join_path2(char *out, size_t out_size, const char *base, const char *suffix)
{
    size_t base_len;
    size_t suffix_len;

    if(out == NULL || out_size == 0 || base == NULL || suffix == NULL)
        return 0;

    base_len = strlen(base);
    suffix_len = strlen(suffix);
    if(base_len + suffix_len >= out_size) {
        out[0] = '\0';
        return 0;
    }

    memcpy(out, base, base_len);
    memcpy(out + base_len, suffix, suffix_len + 1);
    return 1;
}

static int
append_text_checked(char *out, size_t out_size, const char *suffix)
{
    size_t base_len;
    size_t suffix_len;

    if(out == NULL || out_size == 0 || suffix == NULL)
        return 0;

    base_len = strlen(out);
    suffix_len = strlen(suffix);
    if(base_len + suffix_len >= out_size) {
        out[0] = '\0';
        return 0;
    }

    memcpy(out + base_len, suffix, suffix_len + 1);
    return 1;
}

static void
set_status(InbeApp *app, const char *message)
{
    if(app == NULL)
        return;
    copy_text_checked(app->meditation.music_status,
                      sizeof(app->meditation.music_status),
                      message);
}

static int
download_supported(void)
{
#if defined(PLATFORM_WEB)
    return 0;
#else
    return 1;
#endif
}

static const char *
missing_audio_message(void)
{
#if defined(PLATFORM_WEB)
    return "Audio is unavailable";
#else
    return "Download audio before playing music";
#endif
}

static int
is_network_download_error(const FlintRuntimeAssetDownload *download)
{
    const char *error;

    if(download == NULL)
        return 0;
    error = download->error;
    if(error == NULL || error[0] == '\0')
        return 0;

    if(strcmp(error, "NETWORK_UNAVAILABLE") == 0)
        return 1;
    if(strstr(error, "Unable to resolve host") != NULL ||
       strstr(error, "No address associated with hostname") != NULL ||
       strstr(error, "Could not resolve host") != NULL ||
       strstr(error, "Couldn't resolve host") != NULL ||
       strstr(error, "Name or service not known") != NULL ||
       strstr(error, "Temporary failure in name resolution") != NULL)
        return 1;

    return 0;
}

static void
format_download_status(char *out, size_t out_size,
                       const FlintRuntimeAssetDownload *download)
{
    int percent;

    if(out == NULL || out_size == 0 || download == NULL)
        return;

    if(download->status == FLINT_RUNTIME_ASSET_DOWNLOADING &&
       download->total_bytes > 0) {
        percent = (int)((download->bytes * 100u) / download->total_bytes);
        if(percent < 0)
            percent = 0;
        if(percent > 100)
            percent = 100;
        snprintf(out, out_size, "Downloading audio... %d%%", percent);
        return;
    }

    snprintf(out, out_size, "Downloading audio...");
}

static void
draw_download_progress(InbeApp *app, int x, int y, int w)
{
    FlintRuntimeAssetDownload *download;
    int bar_h = flint_px(10);
    int fill_w = 0;
    Color bg = flint_theme_get_bg();
    Color button = flint_theme_get_button();

    if(app == NULL || w <= 0)
        return;

    download = &app->meditation.music_download;
    if(download->status != FLINT_RUNTIME_ASSET_DOWNLOADING)
        return;

    if(download->total_bytes > 0)
        fill_w = (int)((download->bytes * (size_t)w) / download->total_bytes);
    if(fill_w < 0)
        fill_w = 0;
    if(fill_w > w)
        fill_w = w;

    DrawRectangle(x, y, w, bar_h, flint_darken(bg, 28));
    ui_draw_bevel(x, y, w, bar_h, flint_darken(bg, 55), flint_lighten(bg, 35));
    if(fill_w > 0) {
        int inner_w = fill_w - flint_px(4);
        if(inner_w < flint_px(2))
            inner_w = flint_px(2);
        DrawRectangle(x + flint_px(2), y + flint_px(2), inner_w,
                      bar_h - flint_px(4), button);
    }
}

static FlintUIParagraph
music_attribution_paragraph(int content_w)
{
    return (FlintUIParagraph){
        .text = locale_get("meditation_music_attribution"),
        .width = content_w,
        .font = FLINT_TEXT_12,
        .color = flint_darken(flint_theme_get_text(), 34),
    };
}

static int
music_attribution_height(int content_w)
{
    if(content_w <= 0)
        return 0;
    return flint_ui_paragraph_height(music_attribution_paragraph(content_w));
}

static void
draw_music_attribution(int content_x, int content_w, int *y)
{
    if(y == NULL || content_w <= 0)
        return;

    flint_ui_paragraph_draw(music_attribution_paragraph(content_w), content_x, y);
}

static void
music_archive_path(InbeApp *app, char *out, size_t out_size)
{
    if(!join_path2(out, out_size, app->meditation.music_cache_dir,
                   "/inbe-meditation-audio-v1.zip"))
        set_status(app, "Audio cache path is too long");
}

static void
music_track_path(InbeApp *app, int track, char *out, size_t out_size)
{
    if(track < 0 || track >= MEDITATION_MUSIC_TRACK_COUNT)
        track = 0;

#if defined(DEBUG_LOCAL_ASSETS)
    snprintf(out, out_size, "unpackaged_assets/audio/%s", g_tracks[track].file);
    if(file_exists(out))
        return;
#endif

    if(join_path2(out, out_size, app->meditation.music_cache_dir, "/audio/") &&
       append_text_checked(out, out_size, g_tracks[track].file) &&
       file_exists(out))
        return;

#if defined(PLATFORM_WEB)
    out[0] = '\0';
#else
    snprintf(out, out_size, "unpackaged_assets/audio/%s", g_tracks[track].file);
#endif
}

static int
extract_audio_archive(InbeApp *app, const char *archive_path)
{
    mz_zip_archive archive;
    int file_count;
    int extracted = 0;

    memset(&archive, 0, sizeof(archive));
    if(!mz_zip_reader_init_file(&archive, archive_path, 0)) {
        set_status(app, "Failed to open audio archive");
        return 0;
    }

    flint_runtime_asset_ensure_dir(app->meditation.music_cache_dir);
    {
        char audio_dir[FS_PATH_MAX];
        if(join_path2(audio_dir, sizeof(audio_dir), app->meditation.music_cache_dir, "/audio"))
            flint_runtime_asset_ensure_dir(audio_dir);
        else
            set_status(app, "Audio cache path is too long");
    }

    file_count = (int)mz_zip_reader_get_num_files(&archive);
    for(int i = 0; i < file_count; i++) {
        mz_zip_archive_file_stat stat;
        char out_path[FS_PATH_MAX];

        if(!mz_zip_reader_file_stat(&archive, i, &stat))
            continue;
        if(mz_zip_reader_is_file_a_directory(&archive, i))
            continue;
        if(!safe_archive_member(stat.m_filename))
            continue;
        if(!known_track_archive_member(stat.m_filename))
            continue;

        if(!join_path2(out_path, sizeof(out_path), app->meditation.music_cache_dir, "/audio/") ||
           !append_text_checked(out_path, sizeof(out_path), stat.m_filename)) {
            set_status(app, "Audio file path is too long");
            continue;
        }
        {
            char dir[FS_PATH_MAX];
            char *slash;
            if(!copy_text_checked(dir, sizeof(dir), out_path))
                continue;
            slash = strrchr(dir, '/');
            if(slash != NULL) {
                *slash = '\0';
                flint_runtime_asset_ensure_dir(dir);
            }
        }
        if(mz_zip_reader_extract_to_file(&archive, i, out_path, 0))
            extracted++;
    }

    mz_zip_reader_end(&archive);
    snprintf(app->meditation.music_status, sizeof(app->meditation.music_status),
             "Installed %d audio files", extracted);
    return extracted > 0;
}

int
meditation_music_available(InbeApp *app)
{
    char path[FS_PATH_MAX];

    if(app == NULL)
        return 0;

    for(int i = 0; i < MEDITATION_MUSIC_TRACK_COUNT; i++) {
        music_track_path(app, i, path, sizeof(path));
        if(!file_exists(path))
            return 0;
    }

    return 1;
}

const char *
meditation_music_selected_label(InbeApp *app)
{
    int track;
    if(app == NULL)
        return "";
    track = app->meditation.music_track;
    if(track < 0 || track >= MEDITATION_MUSIC_TRACK_COUNT)
        track = 0;
    return g_tracks[track].title;
}

void
meditation_music_init(InbeApp *app)
{
    if(app == NULL)
        return;

    flint_runtime_assets_init("inbe");
    if(!flint_runtime_asset_cache_root("inbe", app->meditation.music_cache_dir,
                                       sizeof(app->meditation.music_cache_dir))) {
        snprintf(app->meditation.music_cache_dir, sizeof(app->meditation.music_cache_dir),
                 "runtime-assets");
        flint_runtime_asset_ensure_dir(app->meditation.music_cache_dir);
    }

    if(app->meditation.music_track < 0 ||
       app->meditation.music_track >= MEDITATION_MUSIC_TRACK_COUNT)
        app->meditation.music_track = 0;

    if(app->meditation.music_status[0] == '\0') {
        set_status(app, meditation_music_available(app) ? "Audio installed" : "Audio not installed");
    }
}

void
meditation_music_unload(InbeApp *app)
{
    if(app == NULL)
        return;
    if(app->meditation.music_loaded) {
        StopMusicStream(app->meditation.music);
        UnloadMusicStream(app->meditation.music);
        app->meditation.music_loaded = 0;
    }
    app->meditation.music_playing = 0;
    app->meditation.music_test_playing = 0;
}

void
meditation_music_stop(InbeApp *app)
{
    if(app == NULL)
        return;
    if(app->meditation.music_loaded)
        StopMusicStream(app->meditation.music);
    app->meditation.music_playing = 0;
    app->meditation.music_test_playing = 0;
}

static void
meditation_music_test_track(InbeApp *app)
{
    int track;

    if(app == NULL)
        return;
    if(app->meditation.music_playing) {
        meditation_music_unload(app);
        set_status(app, "Test audio stopped");
        return;
    }
    if(!meditation_music_available(app)) {
        set_status(app, missing_audio_message());
        return;
    }

    track = app->meditation.music_track;
    if(track < 0 || track >= MEDITATION_MUSIC_TRACK_COUNT)
        track = 0;
    if(!load_track(app, track))
        return;

    PlayMusicStream(app->meditation.music);
    if(!IsMusicStreamPlaying(app->meditation.music))
        TraceLog(LOG_ERROR, "AUDIO: PlayMusicStream returned but test music is not playing");
    app->meditation.music_playing = 1;
    app->meditation.music_test_playing = 1;
    set_status(app, "Playing test audio");
}

static int
load_track(InbeApp *app, int track)
{
    char path[FS_PATH_MAX];

    if(app == NULL)
        return 0;
    if(!app->audio_ready) {
        TraceLog(LOG_ERROR, "AUDIO: Cannot load meditation music because audio device is not ready");
        set_status(app, "Audio device is not ready");
        return 0;
    }

    if(track < 0 || track >= MEDITATION_MUSIC_TRACK_COUNT)
        track = 0;

    meditation_music_unload(app);
    music_track_path(app, track, path, sizeof(path));
    if(!file_exists(path)) {
        set_status(app, "Track is not installed");
        return 0;
    }

    app->meditation.music = LoadMusicStream(path);
    if(app->meditation.music.ctxData == NULL) {
        TraceLog(LOG_ERROR, "AUDIO: Could not load meditation track: %s", path);
        set_status(app, "Could not load track");
        app->meditation.music_loaded = 0;
        return 0;
    }

    app->meditation.music_loaded = 1;
    app->meditation.music_track = track;
    SetMusicVolume(app->meditation.music, (float)app->sound_volume / 100.0f);
    TraceLog(LOG_INFO, "AUDIO: Loaded meditation track: %s", path);
    return 1;
}

void
meditation_music_start_session(InbeApp *app)
{
    int track;

    if(app == NULL || !app->meditation.music_enabled)
        return;
    if(!meditation_music_available(app)) {
        set_status(app, missing_audio_message());
        return;
    }

    track = app->meditation.music_track;
    if(app->meditation.music_shuffle)
        track = GetRandomValue(0, MEDITATION_MUSIC_TRACK_COUNT - 1);
    if(!load_track(app, track))
        return;
    PlayMusicStream(app->meditation.music);
    if(!IsMusicStreamPlaying(app->meditation.music))
        TraceLog(LOG_ERROR, "AUDIO: PlayMusicStream returned but session music is not playing");
    app->meditation.music_playing = 1;
    app->meditation.music_test_playing = 0;
}

void
meditation_music_update(InbeApp *app)
{
    char archive_path[FS_PATH_MAX];

    if(app == NULL)
        return;
    if(app->meditation.music_test_playing &&
       (app->exercise_type != EXERCISE_MEDITATION ||
        (app->inbe.screen != InbeScreenPracticeConfig &&
         app->inbe.screen != InbeScreenManual))) {
        meditation_music_stop(app);
        return;
    }

    if(app->meditation.music_download.status == FLINT_RUNTIME_ASSET_READY &&
       !app->meditation.music_archive_extracted) {
        music_archive_path(app, archive_path, sizeof(archive_path));
        app->meditation.music_archive_extracted = 1;
        extract_audio_archive(app, archive_path);
    } else if(app->meditation.music_download.status == FLINT_RUNTIME_ASSET_ERROR ||
              app->meditation.music_download.status == FLINT_RUNTIME_ASSET_UNSUPPORTED) {
        if(is_network_download_error(&app->meditation.music_download)) {
            set_status(app, locale_get("meditation_music_network_error_title"));
            if(!app->meditation.music_network_error_notified && !app->modal.active) {
                app_open_modal(app, UIModalMeditationNetworkError);
                app->meditation.music_network_error_notified = 1;
            }
        } else if(app->meditation.music_download.error[0] != '\0') {
            set_status(app, app->meditation.music_download.error);
        }
    }

    if(app->meditation.music_loaded) {
        SetMusicVolume(app->meditation.music, (float)app->sound_volume / 100.0f);
        UpdateMusicStream(app->meditation.music);
    }
}

static void
start_download(InbeApp *app)
{
    char archive_path[FS_PATH_MAX];

    if(app == NULL)
        return;

    music_archive_path(app, archive_path, sizeof(archive_path));
    app->meditation.music_archive_extracted = 0;
    app->meditation.music_network_error_notified = 0;
    set_status(app, "Downloading audio...");
    flint_runtime_asset_download(&app->meditation.music_download,
                                 INBE_MEDITATION_AUDIO_URL,
                                 archive_path);
}

void
meditation_music_draw_settings(InbeApp *app, int content_x, int content_w, int *y,
                               int show_installed_download, int show_status)
{
    int toggle_w = flint_px(56);
    int toggle_h = flint_px(30);
    int button_h = flint_px(36);
    int button_w;
    int hover = 0;
    int installed;
    const char *test_label;
    char download_status[96];

    if(app == NULL || y == NULL)
        return;

    if(app->meditation.music_track < 0 ||
       app->meditation.music_track >= MEDITATION_MUSIC_TRACK_COUNT)
        app->meditation.music_track = 0;

    // Master music enable/disable toggle
    flint_text_draw(locale_get("meditation_music_master_toggle"), content_x, *y, flint_ui_font(), flint_theme_get_text());
    if(ui_draw_toggle_switch(content_x, *y + flint_px(26), toggle_w, toggle_h,
                             &app->meditation.music_enabled,
                             locale_get("toggle_off"), locale_get("toggle_on"))) {
        app->settings_dirty = 1;
        meditation_music_unload(app);
    }
    *y += flint_px(76);

    // Only show music options if music is enabled
    if(app->meditation.music_enabled) {
        flint_text_draw(locale_get("meditation_music_shuffle_label"), content_x, *y, flint_ui_font(), flint_theme_get_text());
        if(ui_draw_toggle_switch(content_x, *y + flint_px(26), toggle_w, toggle_h,
                                 &app->meditation.music_shuffle,
                                 locale_get("toggle_off"), locale_get("toggle_on"))) {
            app->settings_dirty = 1;
            meditation_music_unload(app);
        }
        *y += flint_px(76);

        if(!app->meditation.music_shuffle) {
            flint_text_draw(locale_get("meditation_music_track_label"), content_x, *y, flint_ui_font(), flint_theme_get_text());
            if(ui_draw_dropdown_button(401, content_x, *y + flint_px(24), content_w, flint_px(36),
                                       g_track_options, MEDITATION_MUSIC_TRACK_COUNT,
                                       &app->meditation.music_track)) {
                app->settings_dirty = 1;
                meditation_music_unload(app);
            }
            *y += flint_px(74);
        }

        installed = meditation_music_available(app);
        if(installed) {
            test_label = app->meditation.music_playing ?
                             locale_get("meditation_music_stop_test_button") :
                             locale_get("meditation_music_test_button");
            button_w = flint_text_measure(test_label, flint_ui_font()) + flint_px(24);
            if(button_w > content_w)
                button_w = content_w;
            if(ui_draw_generic_button(content_x, *y, button_w, button_h,
                                      test_label,
                                      UI_BUTTON_STYLE_SECONDARY, 0, &hover))
                meditation_music_test_track(app);
            *y += button_h + flint_px(12);
        }

        if(download_supported() && (!installed || show_installed_download)) {
            button_w = flint_text_measure(locale_get(installed ? "meditation_music_redownload_button"
                                                               : "meditation_music_download_button"),
                                          flint_ui_font()) + flint_px(24);
            if(button_w > content_w)
                button_w = content_w;
            if(ui_draw_generic_button(content_x, *y, button_w, button_h,
                                      locale_get(installed ? "meditation_music_redownload_button"
                                                           : "meditation_music_download_button"),
                                      UI_BUTTON_STYLE_PRIMARY, 0, &hover))
                start_download(app);
            *y += button_h + flint_px(12);
        }

        if(show_status && (!installed || app->meditation.music_download.status != FLINT_RUNTIME_ASSET_IDLE)) {
            if(app->meditation.music_download.status == FLINT_RUNTIME_ASSET_DOWNLOADING) {
                format_download_status(download_status, sizeof(download_status),
                                       &app->meditation.music_download);
                flint_text_draw(download_status, content_x, *y, flint_ui_font(), flint_theme_get_text());
                *y += flint_px(28);
                draw_download_progress(app, content_x, *y, content_w);
                *y += flint_px(22);
            } else {
                flint_text_draw(app->meditation.music_status, content_x, *y, flint_ui_font(), flint_theme_get_text());
                *y += flint_px(34);
            }
        }

        draw_music_attribution(content_x, content_w, y);
        *y += flint_px(10);
    }
}

int
meditation_music_measure_settings(InbeApp *app, int content_w,
                                  int show_installed_download, int show_status)
{
    int installed;
    int h = flint_px(76);  // Master toggle

    if(app == NULL)
        return 0;

    // Only measure music options if music is enabled
    if(app->meditation.music_enabled) {
        if(!app->meditation.music_shuffle)
            h += flint_px(74);

        installed = meditation_music_available(app);
        if(installed)
            h += flint_px(48);
        if(download_supported() && (!installed || show_installed_download))
            h += flint_px(48);

        if(show_status && (!installed || app->meditation.music_download.status != FLINT_RUNTIME_ASSET_IDLE)) {
            if(app->meditation.music_download.status == FLINT_RUNTIME_ASSET_DOWNLOADING)
                h += flint_px(50);
            else
                h += flint_px(34);
        }

        h += music_attribution_height(content_w) + flint_px(10);
    }

    return h;
}

int
meditation_music_draw_dropdown_menu(InbeApp *app)
{
    int changed;

    if(app == NULL)
        return 0;

    changed = ui_draw_dropdown_menu(401);
    if(changed) {
        app->settings_dirty = 1;
        meditation_music_unload(app);
    }
    return changed;
}
