#include "app.h"
#include "audio_library.h"
#include "data.h"
#include "practices/meditation/meditation_practice.h"
#include "storage.h"

#include <stdio.h>
#include <string.h>

static const char *const audio_builtin_music_titles[INBE_AUDIO_BUILTIN_MUSIC_COUNT] = {
    "Deep Meditation",
    "Path Of Meditation",
    "Truth Of Silence"
};

static const char *const audio_builtin_music_files[INBE_AUDIO_BUILTIN_MUSIC_COUNT] = {
    "Elijah_K/deep-meditation.ogg",
    "Elijah_K/path-of-meditation.ogg",
    "Elijah_K/truth-of-silence.ogg"
};

static const char *const audio_cue_setting_keys[INBE_AUDIO_CUE_COUNT] = {
    "audio_cue_breath_in",
    "audio_cue_breath_out",
    "audio_cue_bell"
};

static const char *const audio_cue_default_files[INBE_AUDIO_CUE_COUNT] = {
    "breath-in.ogg",
    "breath-out.ogg",
    "bell.ogg"
};

#define INBE_AUDIO_MUSIC_EXTENSIONS ".ogg;.wav;.qoa;.xm;.mod;.mp3;.flac;.m4a;.opus"
#define INBE_AUDIO_SOUND_EXTENSIONS ".ogg;.wav;.qoa;.mp3;.flac;.m4a;.opus"

int
app_audio_music_file_valid(const char *path)
{
    return KryAudioFileValid(path, INBE_AUDIO_MUSIC_EXTENSIONS);
}

int
app_audio_sound_file_valid(const char *path)
{
    return KryAudioFileValid(path, INBE_AUDIO_SOUND_EXTENSIONS);
}

static int
audio_import_error_from_kryon(int error)
{
    switch(error) {
    case KRY_AUDIO_IMPORT_OK: return AUDIO_IMPORT_SUCCESS;
    case KRY_AUDIO_IMPORT_INVALID_PATH: return AUDIO_IMPORT_ERROR_INVALID_PATH;
    case KRY_AUDIO_IMPORT_INVALID_FORMAT: return AUDIO_IMPORT_ERROR_INVALID_FORMAT;
    case KRY_AUDIO_IMPORT_FILE_NOT_FOUND: return AUDIO_IMPORT_ERROR_FILE_NOT_FOUND;
    case KRY_AUDIO_IMPORT_COPY_FAILED: return AUDIO_IMPORT_ERROR_COPY_FAILED;
    case KRY_AUDIO_IMPORT_FULL: return AUDIO_IMPORT_ERROR_UNKNOWN;
    default: return AUDIO_IMPORT_ERROR_UNKNOWN;
    }
}

static int
audio_import_item(InbeAudioLibraryItem *items, int *count, int max_count,
                  const char *kind, const char *src,
                  const char *extensions, int *error_code)
{
    KryAudioLibraryItem item;
    int kry_error = KRY_AUDIO_IMPORT_COPY_FAILED;
    int index;

    if(error_code)
        *error_code = AUDIO_IMPORT_ERROR_UNKNOWN;

    if(items == NULL || count == NULL || extensions == NULL ||
       *count < 0 || *count >= max_count) {
        if(error_code)
            *error_code = AUDIO_IMPORT_ERROR_UNKNOWN;
        return -1;
    }

    index = *count;
    memset(&item, 0, sizeof(item));
    item.title = items[index].title;
    item.title_size = sizeof(items[index].title);
    item.path = items[index].path;
    item.path_size = sizeof(items[index].path);
    if(!KryAudioImportItem(item, index, data_root(), kind, extensions, src,
                           &kry_error)) {
        if(error_code)
            *error_code = audio_import_error_from_kryon(kry_error);
        return -1;
    }
    *count = index + 1;
    if(error_code)
        *error_code = AUDIO_IMPORT_SUCCESS;
    return index;
}

static void
audio_load_item(InbeAudioLibraryItem *item, const char *prefix, int index)
{
    char key[96];
    const char *value;

    if(item == NULL || prefix == NULL)
        return;
    snprintf(key, sizeof(key), "%s_%d_title", prefix, index);
    value = storage_get_setting_text(key);
    snprintf(item->title, sizeof(item->title), "%s",
             value != NULL && value[0] != '\0' ? value : "Custom audio");
    snprintf(key, sizeof(key), "%s_%d_path", prefix, index);
    value = storage_get_setting_text(key);
    snprintf(item->path, sizeof(item->path), "%s", value != NULL ? value : "");
}

static void
audio_save_item(const InbeAudioLibraryItem *item, const char *prefix, int index)
{
    char key[96];

    if(item == NULL || prefix == NULL)
        return;
    snprintf(key, sizeof(key), "%s_%d_title", prefix, index);
    storage_set_setting_text(key, item->title);
    snprintf(key, sizeof(key), "%s_%d_path", prefix, index);
    storage_set_setting_text(key, item->path);
}

void
app_audio_library_load(InbeApp *app)
{
    int i;

    if(app == NULL)
        return;

    app->audio_custom_sound_count =
        storage_get_setting_int("audio_custom_sound_count", 0);
    if(app->audio_custom_sound_count < 0)
        app->audio_custom_sound_count = 0;
    if(app->audio_custom_sound_count > INBE_AUDIO_CUSTOM_SOUND_MAX)
        app->audio_custom_sound_count = INBE_AUDIO_CUSTOM_SOUND_MAX;
    for(i = 0; i < app->audio_custom_sound_count; i++)
        audio_load_item(&app->audio_custom_sounds[i], "audio_custom_sound", i);

    app->audio_custom_music_count =
        storage_get_setting_int("audio_custom_music_count", 0);
    if(app->audio_custom_music_count < 0)
        app->audio_custom_music_count = 0;
    if(app->audio_custom_music_count > INBE_AUDIO_CUSTOM_MUSIC_MAX)
        app->audio_custom_music_count = INBE_AUDIO_CUSTOM_MUSIC_MAX;
    for(i = 0; i < app->audio_custom_music_count; i++)
        audio_load_item(&app->audio_custom_music[i], "audio_custom_music", i);

    for(i = 0; i < INBE_AUDIO_CUE_COUNT; i++) {
        int selected = storage_get_setting_int(audio_cue_setting_keys[i], 0);
        if(selected < 0 || selected > app->audio_custom_sound_count)
            selected = 0;
        app->audio_cue_selected[i] = selected;
    }
}

void
app_audio_library_save(const InbeApp *app)
{
    int i;

    if(app == NULL)
        return;

    storage_set_setting_int("audio_custom_sound_count", app->audio_custom_sound_count);
    for(i = 0; i < INBE_AUDIO_CUSTOM_SOUND_MAX; i++) {
        if(i < app->audio_custom_sound_count) {
            audio_save_item(&app->audio_custom_sounds[i], "audio_custom_sound", i);
        } else {
            InbeAudioLibraryItem empty = {{0}, {0}};
            audio_save_item(&empty, "audio_custom_sound", i);
        }
    }

    storage_set_setting_int("audio_custom_music_count", app->audio_custom_music_count);
    for(i = 0; i < INBE_AUDIO_CUSTOM_MUSIC_MAX; i++) {
        if(i < app->audio_custom_music_count) {
            audio_save_item(&app->audio_custom_music[i], "audio_custom_music", i);
        } else {
            InbeAudioLibraryItem empty = {{0}, {0}};
            audio_save_item(&empty, "audio_custom_music", i);
        }
    }

    for(i = 0; i < INBE_AUDIO_CUE_COUNT; i++)
        storage_set_setting_int(audio_cue_setting_keys[i], app->audio_cue_selected[i]);
}

int
app_audio_import_custom_sound(InbeApp *app, int cue, const char *path)
{
    int error_code;
    return app_audio_import_custom_sound_ex(app, cue, path, &error_code);
}

int
app_audio_import_custom_sound_ex(InbeApp *app, int cue, const char *path, int *error_code)
{
    int index;

    if(error_code)
        *error_code = AUDIO_IMPORT_ERROR_UNKNOWN;

    if(app == NULL) {
        if(error_code)
            *error_code = AUDIO_IMPORT_ERROR_UNKNOWN;
        return 0;
    }

    index = audio_import_item(app->audio_custom_sounds,
                              &app->audio_custom_sound_count,
                              INBE_AUDIO_CUSTOM_SOUND_MAX,
                              "sounds", path,
                              INBE_AUDIO_SOUND_EXTENSIONS, error_code);
    if(index < 0)
        return 0;
    if(cue >= 0 && cue < INBE_AUDIO_CUE_COUNT)
        app->audio_cue_selected[cue] = index + 1;
    app_audio_library_save(app);
    app_audio_reload_cue_sounds(app);
    return 1;
}

int
app_audio_import_custom_music(InbeApp *app, const char *path)
{
    int error_code;
    return app_audio_import_custom_music_ex(app, path, &error_code);
}

int
app_audio_import_custom_music_ex(InbeApp *app, const char *path, int *error_code)
{
    int index;
    Music probe;

    if(error_code)
        *error_code = AUDIO_IMPORT_ERROR_UNKNOWN;

    if(app == NULL) {
        if(error_code)
            *error_code = AUDIO_IMPORT_ERROR_UNKNOWN;
        return 0;
    }

    index = audio_import_item(app->audio_custom_music,
                              &app->audio_custom_music_count,
                              INBE_AUDIO_CUSTOM_MUSIC_MAX,
                              "music", path,
                              INBE_AUDIO_MUSIC_EXTENSIONS, error_code);
    if(index < 0)
        return 0;
    probe = LoadMusicStream(app->audio_custom_music[index].path);
    if(!IsMusicValid(probe)) {
        TraceLog(LOG_ERROR, "AUDIO: Imported music could not be decoded: %s",
                 app->audio_custom_music[index].path);
        remove(app->audio_custom_music[index].path);
        memset(&app->audio_custom_music[index], 0,
               sizeof(app->audio_custom_music[index]));
        app->audio_custom_music_count = index;
        if(error_code)
            *error_code = AUDIO_IMPORT_ERROR_INVALID_FORMAT;
        return 0;
    }
    UnloadMusicStream(probe);
    app->meditation.music_track = INBE_AUDIO_BUILTIN_MUSIC_COUNT + index;
    app_audio_music_sanitize_selection(app);
    app_audio_library_save(app);
    return 1;
}

int
app_audio_remove_custom_sound(InbeApp *app, int index)
{
    int i;
    int cue;

    if(app == NULL || index < 0 || index >= app->audio_custom_sound_count)
        return 0;
    for(i = index; i + 1 < app->audio_custom_sound_count; i++)
        app->audio_custom_sounds[i] = app->audio_custom_sounds[i + 1];
    app->audio_custom_sound_count--;
    memset(&app->audio_custom_sounds[app->audio_custom_sound_count], 0,
           sizeof(app->audio_custom_sounds[0]));
    for(cue = 0; cue < INBE_AUDIO_CUE_COUNT; cue++) {
        if(app->audio_cue_selected[cue] == index + 1)
            app->audio_cue_selected[cue] = 0;
        else if(app->audio_cue_selected[cue] > index + 1)
            app->audio_cue_selected[cue]--;
    }
    app_audio_library_save(app);
    app_audio_reload_cue_sounds(app);
    return 1;
}

int
app_audio_remove_custom_music(InbeApp *app, int index)
{
    int removed_track = INBE_AUDIO_BUILTIN_MUSIC_COUNT + index;
    int i;

    if(app == NULL || index < 0 || index >= app->audio_custom_music_count)
        return 0;
    meditation_music_unload(app);
    for(i = index; i + 1 < app->audio_custom_music_count; i++)
        app->audio_custom_music[i] = app->audio_custom_music[i + 1];
    app->audio_custom_music_count--;
    memset(&app->audio_custom_music[app->audio_custom_music_count], 0,
           sizeof(app->audio_custom_music[0]));
    if(app->meditation.music_track == removed_track)
        app->meditation.music_track = 0;
    else if(app->meditation.music_track > removed_track)
        app->meditation.music_track--;
    for(i = 0; i < EXERCISE_COUNT; i++) {
        if(app->meditation.music_practice_tracks[i] == removed_track)
            app->meditation.music_practice_tracks[i] = 0;
        else if(app->meditation.music_practice_tracks[i] > removed_track)
            app->meditation.music_practice_tracks[i]--;
    }
    app_audio_music_sanitize_selection(app);
    app_audio_library_save(app);
    return 1;
}

int
app_audio_music_count(const InbeApp *app)
{
    int custom = app != NULL ? app->audio_custom_music_count : 0;

    if(custom < 0)
        custom = 0;
    if(custom > INBE_AUDIO_CUSTOM_MUSIC_MAX)
        custom = INBE_AUDIO_CUSTOM_MUSIC_MAX;
    return INBE_AUDIO_BUILTIN_MUSIC_COUNT + custom;
}

const char *
app_audio_music_label(const InbeApp *app, int index)
{
    if(index >= 0 && index < INBE_AUDIO_BUILTIN_MUSIC_COUNT)
        return audio_builtin_music_titles[index];
    index -= INBE_AUDIO_BUILTIN_MUSIC_COUNT;
    if(app != NULL && index >= 0 && index < app->audio_custom_music_count)
        return app->audio_custom_music[index].title;
    return "";
}

const char *
app_audio_cue_default_asset(int cue)
{
    if(cue < 0 || cue >= INBE_AUDIO_CUE_COUNT)
        return "";
    return audio_cue_default_files[cue];
}

int
app_audio_cue_path(InbeApp *app, int cue, char *out, size_t out_size)
{
    int selected;

    if(out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    if(app == NULL || cue < 0 || cue >= INBE_AUDIO_CUE_COUNT)
        return 0;
    selected = app->audio_cue_selected[cue];
    if(selected <= 0 || selected > app->audio_custom_sound_count)
        return 0;
    if(!app_audio_sound_file_valid(app->audio_custom_sounds[selected - 1].path)) {
        app->audio_cue_selected[cue] = 0;
        return 0;
    }
    snprintf(out, out_size, "%s", app->audio_custom_sounds[selected - 1].path);
    return 1;
}

int
app_audio_music_path(const InbeApp *app, int index, char *out, size_t out_size)
{
    if(out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    if(index >= 0 && index < INBE_AUDIO_BUILTIN_MUSIC_COUNT) {
        char candidate[FS_PATH_MAX];
#if defined(DEBUG_LOCAL_ASSETS)
        snprintf(candidate, sizeof(candidate), "unpackaged_assets/audio/%s",
                 audio_builtin_music_files[index]);
        if(app_audio_music_file_valid(candidate)) {
            snprintf(out, out_size, "%s", candidate);
            return 1;
        }
#endif
        if(app != NULL && app->meditation.music_cache_dir[0] != '\0') {
            snprintf(candidate, sizeof(candidate), "%s/audio/%s",
                     app->meditation.music_cache_dir,
                     audio_builtin_music_files[index]);
            if(app_audio_music_file_valid(candidate)) {
                snprintf(out, out_size, "%s", candidate);
                return 1;
            }
        }
        snprintf(candidate, sizeof(candidate), "audio/%s",
                 audio_builtin_music_files[index]);
        if(app_audio_music_file_valid(candidate)) {
            snprintf(out, out_size, "%s", candidate);
            return 1;
        }
        return 0;
    }

    index -= INBE_AUDIO_BUILTIN_MUSIC_COUNT;
    if(app != NULL && index >= 0 && index < app->audio_custom_music_count &&
       app_audio_music_file_valid(app->audio_custom_music[index].path)) {
        snprintf(out, out_size, "%s", app->audio_custom_music[index].path);
        return 1;
    }
    return 0;
}

void
app_audio_music_sanitize_selection(InbeApp *app)
{
    int count;
    int mask;
    int i;

    if(app == NULL)
        return;
    count = app_audio_music_count(app);
    if(count <= 0)
        count = 1;
    if(app->meditation.music_track < 0 || app->meditation.music_track >= count)
        app->meditation.music_track = 0;
    /* The mask is derived from the per-practice track slots so it can never
     * drift out of sync: a bit is set iff that practice has a valid track
     * (i.e. not INBE_AUDIO_MUSIC_NONE). */
    mask = 0;
    for(i = 0; i < EXERCISE_COUNT; i++) {
        int track = app->meditation.music_practice_tracks[i];
        /* INBE_AUDIO_MUSIC_NONE (-1) is valid and means "no music". Any other
         * out-of-range value is clamped to None rather than 0 so a stale index
         * never silently selects a different track. */
        if(track != INBE_AUDIO_MUSIC_NONE &&
           (track < 0 || track >= count))
            app->meditation.music_practice_tracks[i] = INBE_AUDIO_MUSIC_NONE;
        if(app->meditation.music_practice_tracks[i] != INBE_AUDIO_MUSIC_NONE)
            mask |= 1 << i;
    }
    app->meditation.music_practice_mask = mask;
}
