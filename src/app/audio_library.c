#include "app.h"
#include "data.h"
#include "practices/meditation/meditation_practice.h"
#include "storage.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <direct.h>
#endif

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

static int
audio_mkdir(const char *path)
{
    if(path == NULL || path[0] == '\0')
        return 0;
    if(DirectoryExists(path))
        return 1;
#if defined(_WIN32)
    if(_mkdir(path) == 0 || errno == EEXIST)
        return 1;
#else
    if(mkdir(path, 0755) == 0 || errno == EEXIST)
        return 1;
#endif
    return DirectoryExists(path) ? 1 : 0;
}

static int
audio_ensure_library_dir(const char *kind, char *out, size_t out_size)
{
    char root[FS_PATH_MAX];

    if(out == NULL || out_size == 0 || kind == NULL)
        return 0;
    snprintf(root, sizeof(root), "%s/audio", data_root());
    if(!audio_mkdir(root))
        return 0;
    snprintf(out, out_size, "%s/%s", root, kind);
    return audio_mkdir(out);
}

int
app_audio_music_file_valid(const char *path)
{
    FILE *file;

    if(path == NULL || path[0] == '\0')
        return 0;
    /* Support for: ogg, wav, qoa, xm, mod, mp3, flac, m4a, opus */
    if(!IsFileExtension(path, ".ogg;.wav;.qoa;.xm;.mod;.mp3;.flac;.m4a;.opus"))
        return 0;
    file = fopen(path, "rb");
    if(file == NULL)
        return 0;
    fclose(file);
    return 1;
}

int
app_audio_sound_file_valid(const char *path)
{
    FILE *file;

    if(path == NULL || path[0] == '\0')
        return 0;
    /* Support for: ogg, wav, qoa, mp3, flac, m4a, opus */
    if(!IsFileExtension(path, ".ogg;.wav;.qoa;.mp3;.flac;.m4a;.opus"))
        return 0;
    file = fopen(path, "rb");
    if(file == NULL)
        return 0;
    fclose(file);
    return 1;
}

static int
audio_copy_file(const char *src, const char *dst)
{
    FILE *in;
    FILE *out;
    unsigned char buf[8192];
    size_t n;
    int ok = 1;

    if(src == NULL || dst == NULL || src[0] == '\0' || dst[0] == '\0')
        return 0;
    in = fopen(src, "rb");
    if(in == NULL)
        return 0;
    out = fopen(dst, "wb");
    if(out == NULL) {
        fclose(in);
        return 0;
    }
    while((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if(fwrite(buf, 1, n, out) != n) {
            ok = 0;
            break;
        }
    }
    if(ferror(in))
        ok = 0;
    fclose(out);
    fclose(in);
    return ok;
}

static void
audio_title_from_path(char out[INBE_AUDIO_LABEL_SIZE], const char *path)
{
    const char *name;
    const char *ext;
    size_t len;

    if(out == NULL)
        return;
    name = GetFileName(path != NULL ? path : "");
    if(name == NULL || name[0] == '\0')
        name = "Custom audio";
    snprintf(out, INBE_AUDIO_LABEL_SIZE, "%s", name);
    ext = GetFileExtension(out);
    len = strlen(out);
    if(ext != NULL && ext[0] == '.' && strlen(ext) < len)
        out[len - strlen(ext)] = '\0';
    if(out[0] != '\0')
        out[0] = (char)toupper((unsigned char)out[0]);
}

static int
audio_import_item(InbeAudioLibraryItem *items, int *count, int max_count,
                  const char *kind, const char *src,
                  int (*valid)(const char *path), int *error_code)
{
    char dir[FS_PATH_MAX];
    char dst[FS_PATH_MAX];
    char title[INBE_AUDIO_LABEL_SIZE];
    const char *ext;
    int index;

    if(error_code)
        *error_code = AUDIO_IMPORT_ERROR_UNKNOWN;

    if(items == NULL || count == NULL || valid == NULL ||
       *count < 0 || *count >= max_count) {
        if(error_code)
            *error_code = AUDIO_IMPORT_ERROR_UNKNOWN;
        return -1;
    }

    if(src == NULL || src[0] == '\0') {
        if(error_code)
            *error_code = AUDIO_IMPORT_ERROR_INVALID_PATH;
        return -1;
    }

    if(!valid(src)) {
        if(error_code)
            *error_code = AUDIO_IMPORT_ERROR_INVALID_FORMAT;
        return -1;
    }

    if(!audio_ensure_library_dir(kind, dir, sizeof(dir))) {
        if(error_code)
            *error_code = AUDIO_IMPORT_ERROR_COPY_FAILED;
        return -1;
    }

    if(!FileExists(src)) {
        if(error_code)
            *error_code = AUDIO_IMPORT_ERROR_FILE_NOT_FOUND;
        return -1;
    }

    index = *count;
    ext = GetFileExtension(src);
    if(ext == NULL || ext[0] == '\0')
        ext = ".ogg";
    snprintf(dst, sizeof(dst), "%s/custom-%02d%s", dir, index + 1, ext);
    if(!audio_copy_file(src, dst)) {
        if(error_code)
            *error_code = AUDIO_IMPORT_ERROR_COPY_FAILED;
        return -1;
    }

    if(!valid(dst)) {
        if(error_code)
            *error_code = AUDIO_IMPORT_ERROR_INVALID_FORMAT;
        return -1;
    }

    audio_title_from_path(title, src);
    snprintf(items[index].title, sizeof(items[index].title), "%s", title);
    snprintf(items[index].path, sizeof(items[index].path), "%s", dst);
    *count = index + 1;
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
    if(app == NULL)
        return;

    app->audio_custom_sound_count =
        storage_get_setting_int("audio_custom_sound_count", 0);
    if(app->audio_custom_sound_count < 0)
        app->audio_custom_sound_count = 0;
    if(app->audio_custom_sound_count > INBE_AUDIO_CUSTOM_SOUND_MAX)
        app->audio_custom_sound_count = INBE_AUDIO_CUSTOM_SOUND_MAX;
    for(int i = 0; i < app->audio_custom_sound_count; i++)
        audio_load_item(&app->audio_custom_sounds[i], "audio_custom_sound", i);

    app->audio_custom_music_count =
        storage_get_setting_int("audio_custom_music_count", 0);
    if(app->audio_custom_music_count < 0)
        app->audio_custom_music_count = 0;
    if(app->audio_custom_music_count > INBE_AUDIO_CUSTOM_MUSIC_MAX)
        app->audio_custom_music_count = INBE_AUDIO_CUSTOM_MUSIC_MAX;
    for(int i = 0; i < app->audio_custom_music_count; i++)
        audio_load_item(&app->audio_custom_music[i], "audio_custom_music", i);

    for(int i = 0; i < INBE_AUDIO_CUE_COUNT; i++) {
        int selected = storage_get_setting_int(audio_cue_setting_keys[i], 0);
        if(selected < 0 || selected > app->audio_custom_sound_count)
            selected = 0;
        app->audio_cue_selected[i] = selected;
    }
}

void
app_audio_library_save(const InbeApp *app)
{
    if(app == NULL)
        return;

    storage_set_setting_int("audio_custom_sound_count", app->audio_custom_sound_count);
    for(int i = 0; i < INBE_AUDIO_CUSTOM_SOUND_MAX; i++) {
        if(i < app->audio_custom_sound_count) {
            audio_save_item(&app->audio_custom_sounds[i], "audio_custom_sound", i);
        } else {
            InbeAudioLibraryItem empty = {{0}, {0}};
            audio_save_item(&empty, "audio_custom_sound", i);
        }
    }

    storage_set_setting_int("audio_custom_music_count", app->audio_custom_music_count);
    for(int i = 0; i < INBE_AUDIO_CUSTOM_MUSIC_MAX; i++) {
        if(i < app->audio_custom_music_count) {
            audio_save_item(&app->audio_custom_music[i], "audio_custom_music", i);
        } else {
            InbeAudioLibraryItem empty = {{0}, {0}};
            audio_save_item(&empty, "audio_custom_music", i);
        }
    }

    for(int i = 0; i < INBE_AUDIO_CUE_COUNT; i++)
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
                              app_audio_sound_file_valid, error_code);
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
                              app_audio_music_file_valid, error_code);
    if(index < 0)
        return 0;
    app->meditation.music_track = INBE_AUDIO_BUILTIN_MUSIC_COUNT + index;
    app_audio_music_sanitize_selection(app);
    app_audio_library_save(app);
    return 1;
}

int
app_audio_remove_custom_sound(InbeApp *app, int index)
{
    if(app == NULL || index < 0 || index >= app->audio_custom_sound_count)
        return 0;
    for(int i = index; i + 1 < app->audio_custom_sound_count; i++)
        app->audio_custom_sounds[i] = app->audio_custom_sounds[i + 1];
    app->audio_custom_sound_count--;
    memset(&app->audio_custom_sounds[app->audio_custom_sound_count], 0,
           sizeof(app->audio_custom_sounds[0]));
    for(int cue = 0; cue < INBE_AUDIO_CUE_COUNT; cue++) {
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

    if(app == NULL || index < 0 || index >= app->audio_custom_music_count)
        return 0;
    meditation_music_unload(app);
    for(int i = index; i + 1 < app->audio_custom_music_count; i++)
        app->audio_custom_music[i] = app->audio_custom_music[i + 1];
    app->audio_custom_music_count--;
    memset(&app->audio_custom_music[app->audio_custom_music_count], 0,
           sizeof(app->audio_custom_music[0]));
    if(app->meditation.music_track == removed_track)
        app->meditation.music_track = 0;
    else if(app->meditation.music_track > removed_track)
        app->meditation.music_track--;
    for(int i = 0; i < EXERCISE_COUNT; i++) {
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
    for(int i = 0; i < EXERCISE_COUNT; i++) {
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
