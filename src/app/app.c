#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#define MMNOSOUND
#define NOMINMAX
#endif

#include "app.h"
#include "app_sync.h"
#include "data.h"
#include "locale.h"
#include "screens/language_screen.h"
#include "screens/manual_screen.h"
#include "screens/pet_screen.h"
#include "screens/profile_screen.h"
#include "screens/settings/settings_screen.h"
#include "screens/settings/settings_data.h"
#include "screens/settings/settings_sync_account.h"
#include "screens/settings/settings_theme.h"
#include "screens/practice_screen.h"
#include "practices/practice_registry.h"
#include "device_preferences.h"
#include "storage.h"
#include "theme.h"
#include "ui_clip.h"
#include "ui.h"
#include "ui_dpi.h"
#include "ui_text.h"
#include "embedded_assets.h"
#include "practices/meditation/meditation_practice.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#if ANDROID_BUILD
#include "android_wakelock.h"
#include "android_device.h"
void set_global_inbe_app(InbeApp *app);
#endif

#if defined(PLATFORM_WEB)
InbeApp *get_global_inbe_app(void);
#endif

#if defined(PLATFORM_WEB) || ANDROID_BUILD
#define INBE_DEFAULT_WIDTH 320
#define INBE_DEFAULT_HEIGHT 560
#else
#define INBE_DEFAULT_WIDTH 900
#define INBE_DEFAULT_HEIGHT 720
#endif

enum {
    APP_CUE_SAMPLE_RATE = 24000
};

static const float APP_SCREEN_TRANSITION_SECONDS = 0.18f;

#ifndef INBE_PI
#define INBE_PI 3.14159265358979323846f
#endif

InbeConfig config = {
    .title = "Inner Breeze",
    .width = INBE_DEFAULT_WIDTH,
    .height = INBE_DEFAULT_HEIGHT,
    .loaded = 0,
    .title_custom = 1
};

int view_width = INBE_DEFAULT_WIDTH;
int view_height = INBE_DEFAULT_HEIGHT;
static int app_full_view_width = INBE_DEFAULT_WIDTH;
/* Theme colors are now accessed via theme accessor functions */

void
app_leave_practice_config(InbeApp *app)
{
    if(app == NULL)
        return;
    if(app->settings_dirty)
        save_settings(app);
    {
        const PracticeDefinition *practice = practice_get(app->exercise_type);
        if(practice->leave_config != NULL)
            practice->leave_config(app);
    }
    app->settings_scroll = 0;
}

void
app_switch_screen(InbeApp *app, int screen)
{
    if(app == NULL)
        return;
    if(app->inbe.screen == screen &&
       (!app->screen_transition.active ||
        app->screen_transition_target == screen))
        return;

    if(screen == InbeScreenHabits && app->inbe.screen != InbeScreenHabits)
        app->habits.focus_selected_tab = 1;

#if defined(PLATFORM_WEB)
    ResetUITransition(&app->screen_transition);
    app->inbe.screen = screen;
    app->screen_transition_target = screen;
    return;
#else
    if(app->transition_mode == APP_TRANSITION_NONE) {
        ResetUITransition(&app->screen_transition);
        app->inbe.screen = screen;
        app->screen_transition_target = screen;
        return;
    }
#endif

    app->screen_transition_target = screen;
    if(app->screen_transition.active) {
        if(app->inbe.screen != screen)
            ReverseUITransitionToOut(&app->screen_transition);
        return;
    }

    BeginUITransition(&app->screen_transition, APP_SCREEN_TRANSITION_SECONDS);
}

static void
app_advance_screen_transition(InbeApp *app)
{
    int completed_phase;

    if(app == NULL)
        return;
    completed_phase = StepUITransition(&app->screen_transition, GetFrameTime());
    if(completed_phase == UI_TRANSITION_OUT)
        app->inbe.screen = app->screen_transition_target;
    else if(completed_phase == UI_TRANSITION_IN)
        app->screen_transition_target = app->inbe.screen;
}

static void
app_observe_direct_screen_change(InbeApp *app, int before_screen)
{
    if(app == NULL || app->screen_transition.active ||
       before_screen == app->inbe.screen)
        return;

    if(app->transition_mode == APP_TRANSITION_NONE) {
        app->screen_transition_target = app->inbe.screen;
        return;
    }

    app->screen_transition = (UITransition){
        .active = 1,
        .phase = UI_TRANSITION_IN,
        .elapsed_seconds = 0.0f,
        .duration_seconds = APP_SCREEN_TRANSITION_SECONDS
    };
    if(app->inbe.screen == InbeScreenHabits && before_screen != InbeScreenHabits)
        app->habits.focus_selected_tab = 1;
    app->screen_transition_target = app->inbe.screen;
}

static void
app_flush_deferred_settings(InbeApp *app)
{
    if(app == NULL || app->settings_save_delay_ticks <= 0)
        return;

    app->settings_save_delay_ticks--;
    if(app->settings_save_delay_ticks <= 0 && app->settings_dirty)
        save_settings(app);
}

int
app_toolbar_height(void)
{
    return ScaleUIPx(58);
}

int
app_content_top_reserved(const InbeApp *app)
{
    if(app != NULL && app->inbe.screen == InbeScreenStart)
        return GetUITabBarHeight();
    return app_toolbar_height();
}

void
app_open_modal(InbeApp *app, UIModalType type)
{
    if(app == NULL)
        return;
    app->modal.active = 1;
    app->modal.type = type;
    app->modal_input_block_frame = app->inbe.frame;
}

void
app_close_modal(InbeApp *app)
{
    UIModalType type;

    if(app == NULL)
        return;
    type = app->modal.type;
    if(type == UIModalEditProgressiveStartSpeed &&
       app->inbe.screen == InbeScreenStart &&
       app->practice_tab == PRACTICE_TAB_CONFIG) {
        app->modal.type = UIModalPracticeConfig;
        app->modal_input_block_frame = app->inbe.frame;
        return;
    }
    if(type == UIModalPracticeConfig) {
        app_leave_practice_config(app);
        app->settings_scroll = 0;
        app->practice_tab = PRACTICE_TAB_PLAY;
    } else if(type == UIModalPracticeMusic) {
        if(app->settings_dirty)
            save_settings(app);
        app->settings_scroll = 0;
        app->practice_tab = PRACTICE_TAB_PLAY;
    } else if(type == UIModalPracticeManual) {
        app->manual_scroll = 0;
        app->tutorial_step = 0;
        app->practice_tab = PRACTICE_TAB_PLAY;
    }
    app->modal.active = 0;
    app->modal.type = UIModalNone;
    app->modal_input_block_frame = app->inbe.frame;
}

SessionExitModalResult
app_draw_session_exit_modal(int can_save, const char *save_message,
                            const char *discard_message)
{
    int modal_result;

    if(can_save) {
        modal_result = DrawUIModal3Button(GetLocaleText("exit_session_title"),
                                          save_message,
                                          GetLocaleText("cancel_button"),
                                          GetLocaleText("save_button"),
                                          GetLocaleText("discard_button"));
        if(modal_result == 2)
            return SessionExitModalSave;
        if(modal_result == 3)
            return SessionExitModalDiscard;
    } else {
        modal_result = DrawUIModal(GetLocaleText("exit_session_title"),
                                     discard_message,
                                     GetLocaleText("cancel_button"),
                                     GetLocaleText("exit_button"));
        if(modal_result == 2)
            return SessionExitModalDiscard;
    }
    return modal_result == 1 ? SessionExitModalCancel : SessionExitModalNone;
}

static int
exercise_manual_seen_bit(int exercise_type)
{
    if(exercise_type < 0 || exercise_type >= EXERCISE_COUNT)
        return 0;
    return 1 << exercise_type;
}

int
exercise_manual_seen(InbeApp *app, int exercise_type)
{
    int bit = exercise_manual_seen_bit(exercise_type);
    if(app == NULL || bit == 0)
        return 1;
    return (app->exercise_manual_seen_mask & bit) != 0;
}

void
mark_exercise_manual_seen(InbeApp *app, int exercise_type)
{
    int bit = exercise_manual_seen_bit(exercise_type);
    if(app == NULL || bit == 0)
        return;
    if((app->exercise_manual_seen_mask & bit) == 0) {
        app->exercise_manual_seen_mask |= bit;
        save_settings(app);
    }
}

static int
load_locale_font(InbeApp *app)
{
    Font font;
    Image white;
    const EmbeddedAsset *png;
    const EmbeddedAsset *dat;

    if(app == NULL)
        return 0;

    png = GetEmbeddedAsset("assets/fonts/locales.png");
    dat = GetEmbeddedAsset("assets/fonts/locales.dat");
    if(png == NULL || dat == NULL)
        return 0;

    font = LoadUIChoppedFontFromMemory(png->data, png->size,
                                                    dat->data, dat->size,
                                                    UI_TEXT_BASE_SIZE);
    if(font.texture.id == 0)
        return 0;

    white = GenImageColor(1, 1, WHITE);
    app->font_shapes_texture = LoadTextureFromImage(white);
    UnloadImage(white);
    if(app->font_shapes_texture.id == 0) {
        UnloadUIFont(&font);
        goto fail;
    }
    SetTextureFilter(app->font_shapes_texture, TEXTURE_FILTER_POINT);

    app->locale_font = font;
    app->locale_font_8 = font;
    SetUIFont(app->locale_font);
    SetUISmallFont(app->locale_font);
    SetShapesTexture(app->font_shapes_texture, (Rectangle){0, 0, 1, 1});
    return 1;

fail:
    app->locale_font = (Font){0};
    app->locale_font_8 = (Font){0};
    return 0;
}

static void
unload_locale_font(InbeApp *app)
{
    if(app == NULL)
        return;

    SetUIFont((Font){0});
    SetUISmallFont((Font){0});
    UnloadUIFont(&app->locale_font);
    app->locale_font = (Font){0};
    app->locale_font_8 = (Font){0};
}

static void
discard_locale_font_cpu(InbeApp *app)
{
    if(app == NULL)
        return;
    free(app->locale_font.glyphs);
    free(app->locale_font.recs);
    app->locale_font = (Font){0};
    app->locale_font_8 = (Font){0};
    SetUIFont((Font){0});
    SetUISmallFont((Font){0});
}

static void
app_reload_graphics_resources(InbeApp *app)
{
    if(app == NULL || !app->graphics_reload_requested)
        return;

    app->graphics_reload_requested = 0;
    TraceLog(LOG_INFO, "ANDROID: Reloading graphics resources");

    for(int i = 0; i < UI_ICON_TYPE_COUNT; i++)
        app->icons[i] = (Texture2D){0};
    LoadAllUIIconTextures(app->icons);
    SetUIIcons(app->icons[UI_ICON_TYPE_GEAR], app->icons[UI_ICON_TYPE_X]);

    app->font_shapes_texture = (Texture2D){0};
    discard_locale_font_cpu(app);
    if(!load_locale_font(app))
        TraceLog(LOG_WARNING, "FONT: Failed to reload chopped locale font");
}

void
refresh_locale_dependent_text(InbeApp *app)
{
    if(app == NULL)
        return;

    if(!config.title_custom) {
        snprintf(config.title, sizeof(config.title), "%s", GetLocaleText("app_title"));
    }
    manual_screen_reset_layouts(app);
    app->language_index = GetCurrentLocaleIndex();
    if(app->language_index < 0)
        app->language_index = 0;
}

void
apply_language_selection(InbeApp *app, int language_index, int save_now)
{
    const char *code;

    if(app == NULL)
        return;

    if(language_index < 0 || language_index >= GetLocaleCount())
        language_index = 0;

    code = GetLocaleCode(language_index);
    if(code == NULL || code[0] == '\0')
        code = "en";

    if(!SetLocale(code)) {
        code = "en";
        SetLocale(code);
    }

    snprintf(app->language, sizeof(app->language), "%s", code);
    app->language_selected = 1;
    refresh_locale_dependent_text(app);

    if(save_now)
        save_settings(app);
}

#if defined(PLATFORM_WEB)
#include <emscripten.h>

static int web_storage_ready = 0;

static void
init_web_storage(void)
{
    int ok;

    if(web_storage_ready)
        return;

    ok = EM_ASM_INT({
        if(typeof FS === 'undefined' || typeof IDBFS === 'undefined')
            return 0;

        try {
            FS.mkdir('/home');
        } catch(e) {}

        try {
            if(!FS.analyzePath('/home').object.isFolder) return 0;
            FS.mount(IDBFS, {root: '/'}, '/home');
        } catch(e) {
            if(e.errno !== 10 && String(e).indexOf('already mounted') === -1) {
                console.error('IDBFS mount failed:', e);
                return 0;
            }
        }

        return 1;
    });
    web_storage_ready = ok != 0;
}

#endif

static void
load_config(void)
{
    if(config.loaded)
        return;

    if(config.title[0] == '\0') {
        snprintf(config.title, sizeof(config.title), "%s", GetLocaleText("app_title"));
        config.title_custom = 0;
    }

#if defined(PLATFORM_WEB)
    refresh_theme_colors(THEME_SKY, 1);
#else
    refresh_theme_colors(THEME_SKY, 0);
#endif

    config.loaded = 1;
}

int
clampi(int value, int min, int max)
{
    if(value < min)
        return min;
    if(value > max)
        return max;
    return value;
}

void
count_from_int(char dst[CountSize], int value)
{
    value = clampi(value, 0, 999);
    dst[0] = (char)('0' + (value / 100) % 10);
    dst[1] = (char)('0' + (value / 10) % 10);
    dst[2] = (char)('0' + value % 10);
    dst[3] = 0;
}

int
int_from_count(const char src[CountSize])
{
    int a = (src[0] >= '0' && src[0] <= '9') ? src[0] - '0' : 0;
    int b = (src[1] >= '0' && src[1] <= '9') ? src[1] - '0' : 0;
    int c = (src[2] >= '0' && src[2] <= '9') ? src[2] - '0' : 0;
    return a * 100 + b * 10 + c;
}

static void
app_restore_habits_view_settings(InbeApp *app)
{
    const char *selected_id;
    int screen_mode;
    int habit_tab;
    int view_mode;

    if(app == NULL)
        return;

    screen_mode = storage_get_setting_int("habits_screen_mode", HABITS_SCREEN_OVERVIEW);
    habit_tab = storage_get_setting_int("habits_tab", HABIT_TAB_WEEKLY);
    view_mode = storage_get_setting_int("habits_view_mode", HABIT_VIEW_WEEKLY);
    app->habits.screen_mode = clampi(screen_mode, HABITS_SCREEN_OVERVIEW,
                                     HABITS_SCREEN_REORDER);
    if(app->habits.screen_mode == HABITS_SCREEN_REORDER)
        app->habits.screen_mode = HABITS_SCREEN_OVERVIEW;
    app->habits.tab = clampi(habit_tab, HABIT_TAB_WEEKLY, HABIT_TAB_COUNT - 1);
    app->habits.view_mode = clampi(view_mode, HABIT_VIEW_CALENDAR, HABIT_VIEW_WEEKLY);

    selected_id = storage_get_setting_text("habits_selected_id");
    if(selected_id != NULL && selected_id[0] != '\0') {
        for(int i = 0; i < app->habits.count; i++) {
            if(strcmp(app->habits.items[i].id, selected_id) == 0) {
                app->habits.selected = i;
                break;
            }
        }
    }
    if(app->habits.selected < 0 || app->habits.selected >= app->habits.count)
        app->habits.selected = app->habits.count > 0 ? 0 : -1;
}

void
app_reload_after_import(InbeApp *app, int reload_settings)
{
    char selected_habit_id[INBE_HABIT_ID_SIZE] = "";
    char detail_habit_id[INBE_HABIT_ID_SIZE] = "";
    int selected = -1;
    int detail_index = -1;
    int view_mode = HABIT_VIEW_CALENDAR;
    int habit_tab = HABIT_TAB_WEEKLY;
    int month_offset = 0;
    int scroll = 0;
    int weekly_days = 0;
    int hold_stats_range_days = 0;
    if(app == NULL)
        return;

    if(!reload_settings) {
        if(app->habits.selected >= 0 && app->habits.selected < app->habits.count)
            snprintf(selected_habit_id, sizeof(selected_habit_id), "%s",
                     app->habits.items[app->habits.selected].id);
        if(app->habit_detail_index >= 0 && app->habit_detail_index < app->habits.count)
            snprintf(detail_habit_id, sizeof(detail_habit_id), "%s",
                     app->habits.items[app->habit_detail_index].id);
        selected = app->habits.selected;
        detail_index = app->habit_detail_index;
        view_mode = app->habits.view_mode;
        habit_tab = app->habits.tab;
        month_offset = app->habits.month_offset;
        scroll = app->habits.scroll;
        weekly_days = app->habits.weekly_days;
        hold_stats_range_days = app->habits.hold_stats_range_days;
    }

    if(reload_settings) {
        if(app_load_settings(app))
            save_settings(app);
        reset_settings_preview(app);
        practice_update_session_sounds(app);
    }

    habits_init(&app->habits);
    if(reload_settings)
        app_restore_habits_view_settings(app);
    if(!reload_settings) {
        app->habits.selected = -1;
        for(int i = 0; i < app->habits.count; i++) {
            if(selected_habit_id[0] != '\0' &&
               strcmp(app->habits.items[i].id, selected_habit_id) == 0) {
                app->habits.selected = i;
                break;
            }
        }
        if(app->habits.selected < 0 && selected >= 0 && selected < app->habits.count)
            app->habits.selected = selected;
        app->habit_detail_index = -1;
        for(int i = 0; i < app->habits.count; i++) {
            if(detail_habit_id[0] != '\0' &&
               strcmp(app->habits.items[i].id, detail_habit_id) == 0) {
                app->habit_detail_index = i;
                break;
            }
        }
        if(app->habit_detail_index < 0 && detail_index >= 0 && detail_index < app->habits.count)
            app->habit_detail_index = detail_index;
        app->habits.view_mode = view_mode;
        app->habits.tab = habit_tab;
        app->habits.month_offset = month_offset;
        app->habits.scroll = scroll;
        app->habits.weekly_days = weekly_days;
        app->habits.hold_stats_range_days = hold_stats_range_days == 31 ? 31 : 7;
    }
    if(app->habit_detail_index >= app->habits.count)
        app->habit_detail_index = -1;
    if(app->habits.selected < 0 || app->habits.selected >= app->habits.count)
        app->habits.selected = app->habits.count > 0 ? 0 : -1;
    app->habit_session_edit.active = 0;
    app->habit_edit.active = 0;
}

#if ANDROID_BUILD
void
app_request_graphics_reload(InbeApp *app)
{
    if(app != NULL)
        app->graphics_reload_requested = 1;
}
#endif

static Texture2D
load_pixel_texture_from_asset(const char *path)
{
    const EmbeddedAsset *asset = GetEmbeddedAsset(path);
    Image image;
    Texture2D texture = {0};

    if(asset == NULL || asset->data == NULL || asset->size == 0)
        return texture;

    image = LoadImageFromMemory(GetEmbeddedAssetExtension(path), asset->data, (int)asset->size);
    if(image.data == NULL)
        return texture;

#if defined(_WIN32)
    {
        int pot_w = 1;
        int pot_h = 1;
        while(pot_w < image.width)
            pot_w <<= 1;
        while(pot_h < image.height)
            pot_h <<= 1;
        if(pot_w != image.width || pot_h != image.height)
            ImageResizeCanvas(&image, pot_w, pot_h, 0, 0, BLANK);
    }
#endif

    texture = LoadTextureFromImage(image);
    UnloadImage(image);
    if(texture.id != 0)
        SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    return texture;
}

Texture2D
app_load_asset_texture(const char *name)
{
    char path[64];

    snprintf(path, sizeof(path), "assets/%s", name);
    return load_pixel_texture_from_asset(path);
}

static Sound
load_sound_asset(const char *name)
{
    char path[96];
    const EmbeddedAsset *asset;
    Wave wave;
    Sound sound = {0};

    snprintf(path, sizeof(path), "assets/sounds/%s", name);
    asset = GetEmbeddedAsset(path);
    if(asset == NULL || asset->data == NULL || asset->size == 0) {
        TraceLog(LOG_ERROR, "AUDIO: Missing embedded sound asset: %s", path);
        return sound;
    }

    wave = LoadWaveFromMemory(GetEmbeddedAssetExtension(path), asset->data, (int)asset->size);
    if(wave.data == NULL) {
        TraceLog(LOG_ERROR, "AUDIO: Failed to decode embedded sound asset: %s", path);
        return sound;
    }

    sound = LoadSoundFromWave(wave);
    UnloadWave(wave);
    if(sound.frameCount == 0)
        TraceLog(LOG_ERROR, "AUDIO: Failed to create sound from embedded asset: %s", path);
    else
        TraceLog(LOG_INFO, "AUDIO: Loaded sound asset %s (%u frames)", path, sound.frameCount);
    return sound;
}

static float
app_sound_volume_scale(InbeApp *app, float scale)
{
    float volume;

    if(app == NULL || app->sound_volume <= 0)
        return 0.0f;

    volume = ((float)app->sound_volume / 100.0f) * scale;
    if(volume < 0.0f)
        volume = 0.0f;
    if(volume > 1.0f)
        volume = 1.0f;
    return volume;
}

static float
cue_smoothstep(float t)
{
    if(t < 0.0f)
        t = 0.0f;
    if(t > 1.0f)
        t = 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

static float
breath_cue_envelope(float t)
{
    float attack = cue_smoothstep(t / 0.22f);
    float release = cue_smoothstep((1.0f - t) / 0.34f);
    return attack < release ? attack : release;
}

static Sound
load_generated_breath_cue(int dir, float duration_seconds)
{
    unsigned int frame_count;
    float *samples;
    Wave wave;
    Sound sound = {0};
    float phase = 0.0f;

    if(duration_seconds < 0.75f)
        duration_seconds = 0.75f;
    if(duration_seconds > 3.4f)
        duration_seconds = 3.4f;

    frame_count = (unsigned int)(duration_seconds * (float)APP_CUE_SAMPLE_RATE + 0.5f);
    samples = malloc(sizeof(float) * frame_count);
    if(samples == NULL)
        return sound;

    for(unsigned int i = 0; i < frame_count; i++) {
        float t = frame_count > 1 ? (float)i / (float)(frame_count - 1) : 0.0f;
        float drift = dir == 0 ? t : 1.0f - t;
        float freq = (dir == 0 ? 196.0f : 174.0f) + 42.0f * cue_smoothstep(drift);
        float env = breath_cue_envelope(t);

        phase += (2.0f * INBE_PI * freq) / (float)APP_CUE_SAMPLE_RATE;
        samples[i] = sinf(phase) * env * 0.22f;
    }

    wave = (Wave){
        .frameCount = frame_count,
        .sampleRate = APP_CUE_SAMPLE_RATE,
        .sampleSize = 32,
        .channels = 1,
        .data = samples
    };
    sound = LoadSoundFromWave(wave);
    free(samples);
    return sound;
}

static Sound
load_generated_bell_cue(void)
{
    enum { BELL_SECONDS = 6 };
    unsigned int frame_count = APP_CUE_SAMPLE_RATE * BELL_SECONDS;
    float *samples = malloc(sizeof(float) * frame_count);
    Wave wave;
    Sound sound = {0};

    if(samples == NULL)
        return sound;

    for(unsigned int i = 0; i < frame_count; i++) {
        float t = (float)i / (float)APP_CUE_SAMPLE_RATE;
        float strike = cue_smoothstep((0.045f - t) / 0.045f);
        float body = expf(-t * 0.82f);
        float high = expf(-t * 1.65f);
        float sample =
            sinf(2.0f * INBE_PI * 432.0f * t) * body * 0.38f +
            sinf(2.0f * INBE_PI * 648.0f * t) * body * 0.22f +
            sinf(2.0f * INBE_PI * 864.0f * t) * high * 0.14f +
            sinf(2.0f * INBE_PI * 1296.0f * t) * high * 0.06f;

        samples[i] = sample * (0.18f + 0.82f * (1.0f - strike));
    }

    wave = (Wave){
        .frameCount = frame_count,
        .sampleRate = APP_CUE_SAMPLE_RATE,
        .sampleSize = 32,
        .channels = 1,
        .data = samples
    };
    sound = LoadSoundFromWave(wave);
    free(samples);
    return sound;
}

static int
app_lerp_int(int a, int b, int num, int den)
{
    int delta = b - a;
    int scaled = delta * num;

    if(scaled >= 0)
        scaled += den / 2;
    else
        scaled -= den / 2;

    return a + scaled / den;
}

static int
app_effective_breath_half_ticks(const Inbe *inbe)
{
    int target_ticks;
    int start_speed;
    int start_ticks;
    int completed_breaths;

    if(inbe == NULL)
        return breath_half_ticks_for_speed(DefaultSpeedLevel);

    target_ticks = breath_half_ticks_for_speed(inbe->speed_level);
    start_speed = inbe->progressive_start_speed;

    if(!inbe->progressive_speed || inbe->round != 0)
        return target_ticks;

    if(start_speed < SETTINGS_SPEED_MIN)
        start_speed = SETTINGS_SPEED_MIN;
    if(start_speed > inbe->speed_level)
        start_speed = inbe->speed_level;

    completed_breaths = int_from_count(inbe->count);
    start_ticks = breath_half_ticks_for_speed(start_speed);
    if(completed_breaths < 5)
        return start_ticks;
    if(completed_breaths >= 10)
        return target_ticks;

    return app_lerp_int(start_ticks, target_ticks, completed_breaths - 4, 5);
}

static int
app_breath_cue_index(const Inbe *inbe)
{
    int half_ticks = app_effective_breath_half_ticks(inbe);
    int best = 0;
    int best_delta = 100000;

    for(int i = 0; i < SETTINGS_SPEED_MAX; i++) {
        int delta = breath_half_ticks_for_speed(i + 1) - half_ticks;
        if(delta < 0)
            delta = -delta;
        if(delta < best_delta) {
            best = i;
            best_delta = delta;
        }
    }
    return best;
}

void
app_play_sound(InbeApp *app, Sound sound, float scale)
{
    float volume;

    if(app == NULL)
        return;
    if(!app->audio_ready) {
        TraceLog(LOG_ERROR, "AUDIO: Cannot play sound because audio device is not ready");
        return;
    }
    if(sound.frameCount == 0) {
        TraceLog(LOG_ERROR, "AUDIO: Cannot play sound because sound is not loaded");
        return;
    }
    volume = app_sound_volume_scale(app, scale);
    if(volume <= 0.0f)
        return;

    StopSound(sound);
    SetSoundVolume(sound, volume);
    PlaySound(sound);
    if(!IsSoundPlaying(sound))
        TraceLog(LOG_ERROR, "AUDIO: PlaySound returned but sound is not playing");
}

void
app_play_breath_cue(InbeApp *app, int dir)
{
    Sound sound;

    if(app == NULL)
        return;

    sound = dir == 0
                ? app->breath_in_cue_sounds[app_breath_cue_index(&app->inbe)]
                : app->breath_out_cue_sounds[app_breath_cue_index(&app->inbe)];
    if(sound.frameCount != 0) {
        app_play_sound(app, sound, 0.72f);
        return;
    }

    app_play_sound(app, dir == 0 ? app->breath_in_sound : app->breath_out_sound, 1.0f);
}

void
app_play_bell_cue(InbeApp *app, float scale)
{
    if(app == NULL)
        return;
    if(app->bell_cue_sound.frameCount != 0) {
        app_play_sound(app, app->bell_cue_sound, scale);
        return;
    }
    app_play_sound(app, app->bell_sound, scale);
}

static void
init_audio(InbeApp *app)
{
    if(app == NULL || app->audio_ready)
        return;

    InitAudioDevice();
    app->audio_ready = IsAudioDeviceReady();
    if(!app->audio_ready) {
        TraceLog(LOG_ERROR, "AUDIO: Audio device failed to initialize");
        return;
    }
    TraceLog(LOG_INFO, "AUDIO: Audio device initialized");

    app->breath_in_sound = load_sound_asset("breath-in.ogg");
    app->breath_out_sound = load_sound_asset("breath-out.ogg");
    app->bell_sound = load_sound_asset("bell.ogg");
    for(int i = 0; i < SETTINGS_SPEED_MAX; i++) {
        float seconds = (float)breath_half_ticks_for_speed(i + 1) / 60.0f;
        app->breath_in_cue_sounds[i] = load_generated_breath_cue(0, seconds);
        app->breath_out_cue_sounds[i] = load_generated_breath_cue(1, seconds);
    }
    app->bell_cue_sound = load_generated_bell_cue();
}

void
app_init(void *vapp) {
    InbeApp *app = vapp;
    if(app == 0)
        return;

    app->locale_font = (Font){0};
    app->locale_font_8 = (Font){0};

#if ANDROID_BUILD
    if (practice_active(app) != NULL) {
        android_allow_screen_off();
    }
    practice_active_background_stop(app);
#endif

#if defined(PLATFORM_WEB)
    init_web_storage();
#endif
    InitLocale();
    if(!load_locale_font(app)) {
        TraceLog(LOG_WARNING, "FONT: Failed to load chopped locale font -> using built-in default");
    }
    load_config();

    view_width = config.width > 0 ? config.width : INBE_DEFAULT_WIDTH;
    view_height = config.height > 0 ? config.height : INBE_DEFAULT_HEIGHT;
    UpdateUIDPI(view_width, view_height);
    InitUI(view_width, view_height, GetUIDPIScale());
#if ANDROID_BUILD
    SetUITextInputPlatformCallback(android_device_set_soft_keyboard_visible);
#endif

    inbeinit(&app->inbe);
    data_init();
    if(app_load_settings(app))
        save_settings(app);
    app->practice_tab = PRACTICE_TAB_PLAY;
    app->practice_config_tab = 0;
    if(app->language_needs_save) {
        save_settings(app);
        app->language_needs_save = 0;
    }
    practice_update_circle_bounds(app, app_content_top_reserved(app),
                                  app_content_bottom_reserved(app));
    habits_init(&app->habits);
    app_restore_habits_view_settings(app);
    app->habit_detail_index = -1;
    app->habit_session_edit = (HabitSessionEditState){.round = -1};
    ResetUITransition(&app->screen_transition);
    app->inbe.screen = app->main_tab == APP_MAIN_TAB_HABITS
                           ? InbeScreenHabits
                           : InbeScreenStart;
    app->habits.focus_selected_tab = app->inbe.screen == InbeScreenHabits;
    app->screen_transition_target = app->inbe.screen;
    init_audio(app);
    for(int i = 0; i < practice_count(); i++) {
        const PracticeDefinition *practice = practice_get(i);
        if(practice->init != NULL)
            practice->init(app);
    }
    app->camera = (Camera2D){0};
    app->cursor_clickable = 0;
    app->cursor_disabled = 0;
    app->play_circle_hover = 0;
    app->play_circle_scale = 1.0f;
    app->settings_tab = SETTINGS_TAB_OVERVIEW;
    app->habit_edit = (HabitEditState){
        .index = -1,
        .color = {99, 196, 165, 255},
        .sync_mode = INBE_HABIT_SYNC_NONE
    };
    practice_update_session_sounds(app);
    reset_settings_preview(app);
    inbeinit(&app->start_speed_preview);

    // Load all icons
    LoadAllUIIconTextures(app->icons);

    SetUIIcons(app->icons[UI_ICON_TYPE_GEAR], app->icons[UI_ICON_TYPE_X]);

    if(!app->language_selected)
        app->inbe.screen = InbeScreenLanguage;
    else
        app->inbe.screen = InbeScreenStart;
    app->screen_transition_target = app->inbe.screen;

    app->modal = (UIModal){0};
    app->meditation.duration_seconds = 0;
    app->meditation.remaining_seconds = 0;
    app->meditation.frame_ticks = 0;
    app_auto_sync(app);
}

#if defined(PLATFORM_WEB)
EMSCRIPTEN_KEEPALIVE
int
app_web_get_play_in_background(void)
{
    InbeApp *app = get_global_inbe_app();
    return app != NULL && app->inbe.play_in_background;
}

EMSCRIPTEN_KEEPALIVE
void
app_web_set_backgrounded(int active)
{
    InbeApp *app = get_global_inbe_app();

    if(app == NULL)
        return;

    app->backgrounded = active ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
void
app_web_background_tick(int elapsed_ms)
{
    InbeApp *app = get_global_inbe_app();

    if(app == NULL || elapsed_ms <= 0)
        return;
    app->backgrounded = 1;
    practice_active_advance_elapsed(app, elapsed_ms);
}
#endif

static void
handle_back_button(InbeApp *app)
{
    /* If modal is active, let modal drawing handle it */
    if(app->modal.active) {
        return;
    }

    switch(app->inbe.screen) {
    case InbeScreenStart:
        if(app->practice_tab == PRACTICE_TAB_CONFIG)
            app_leave_practice_config(app);
        if(app->practice_tab != PRACTICE_TAB_PLAY) {
            app->practice_tab = PRACTICE_TAB_PLAY;
            app->manual_scroll = 0;
            app->settings_scroll = 0;
            app->tutorial_step = 0;
            break;
        }
        break;

    case InbeScreenSettings:
        if(app->settings_dirty)
            save_settings(app);
        settings_screen_clear_status();
        app_switch_screen(app, app->main_tab == APP_MAIN_TAB_HABITS
                                  ? InbeScreenHabits
                                  : InbeScreenStart);
        app->settings_scroll = 0;
        break;

    case InbeScreenProfile:
        if(app->profile_view != PROFILE_VIEW_MAIN) {
            app->profile_view = PROFILE_VIEW_MAIN;
            app->profile_scroll = 0;
            app->sync_server_url_focused = 0;
            settings_screen_clear_status();
            break;
        }
        if(app->profile_tab != PROFILE_TAB_OVERVIEW) {
            app->profile_tab = PROFILE_TAB_OVERVIEW;
            app->profile_scroll = 0;
            settings_screen_clear_status();
            break;
        }
        app_switch_screen(app, app->main_tab == APP_MAIN_TAB_HABITS
                                  ? InbeScreenHabits
                                  : InbeScreenStart);
        break;

    case InbeScreenPet:
        app_switch_screen(app, app->main_tab == APP_MAIN_TAB_HABITS
                                  ? InbeScreenHabits
                                  : InbeScreenStart);
        break;

    case InbeScreenPracticeConfig:
        app_leave_practice_config(app);
        app_switch_screen(app, InbeScreenStart);
        break;

    case InbeScreenHabits:
        if(app->habits.tab == HABIT_TAB_EDIT && app->habit_edit.active) {
            habit_edit_commit(app);
            break;
        }
        app_switch_screen(app, InbeScreenStart);
        break;

    case InbeScreenHabitEdit:
        habit_edit_cancel(app);
        app_switch_screen(app, InbeScreenHabits);
        break;

    case InbeScreenHabitSessionEdit:
        habit_session_cancel_edit(app);
        app_switch_screen(app, InbeScreenHabits);
        break;

    case InbeScreenLanguage:
        break;

    case InbeScreenManual:
        manual_screen_close_tutorial(app, 0);
        break;

    case InbeScreenResults:
        practice_ensure_results_saved(app);
#if ANDROID_BUILD
        android_allow_screen_off();
        android_wakelock_release();
#endif
        app_init(app);
        break;

    case InbeScreenSession:
        /* When paused, exit immediately */
        if(app->session_paused) {
            practice_active_background_stop(app);
            app_init(app);
        } else {
            /* Show confirmation modal */
            app_open_modal(app, UIModalConfirmExitSession);
        }
        break;

    case InbeScreenMeditation:
        {
            const PracticeDefinition *practice = practice_get(PRACTICE_MEDITATION);
            if(practice->request_exit != NULL)
                practice->request_exit(app);
        }
        break;

    default:
        break;
    }
}

static void
draw_global_modal(InbeApp *app)
{
    int modal_result;

    if(app == NULL || !app->modal.active)
        return;
    if(app->modal_input_block_frame == app->inbe.frame)
        return;

    ClearUIInputCaptures();

    if(settings_data_draw_modals(app))
        return;

    if(app->modal.type == UIModalMeditationNetworkError) {
        modal_result = DrawUIModal(GetLocaleText("meditation_music_network_error_title"),
                                     GetLocaleText("meditation_music_network_error_message"),
                                     GetLocaleText("ok_button"),
                                     GetLocaleText("ok_button"));
        if(modal_result != 0) {
            app_close_modal(app);
        }
    }
    if(app->modal.type == UIModalThemePicker)
        settings_screen_draw_theme_picker_modal(app);
    if(app->modal.type == UIModalConfirmRemoveFriend) {
        profile_screen_draw_remove_friend_modal(app);
        return;
    }
    if(app->modal.type == UIModalPracticeManual ||
       app->modal.type == UIModalPracticeConfig ||
       app->modal.type == UIModalPracticeMusic ||
       app->modal.type == UIModalEditProgressiveStartSpeed) {
        practice_screen_draw_modal(app);
        return;
    }
    if(app->modal.type == UIModalSyncAlias) {
        modal_result = settings_sync_account_draw_alias_modal(app);
        if(modal_result == 1) {
            int then_backup = app->sync_alias_then_backup;
            app->sync_alias_then_backup = 0;
            app_close_modal(app);
            settings_screen_set_status_success(GetLocaleText("sync_alias_saved"), NULL);
            if(then_backup)
                app_open_modal(app, UIModalSyncAccountBackup);
        } else if(modal_result == 2) {
            settings_screen_set_status_error(GetLocaleText("sync_alias_failed"));
        } else if(modal_result == 3) {
            int then_backup = app->sync_alias_then_backup;
            app->sync_alias_then_backup = 0;
            app_close_modal(app);
            if(then_backup)
                app_open_modal(app, UIModalSyncAccountBackup);
        }
    }
    if(app->modal.type == UIModalSyncPublicId) {
        modal_result = settings_sync_account_draw_public_id_modal(app);
        if(modal_result != 0)
            app_close_modal(app);
    }
}

#if !ANDROID_BUILD && !defined(PLATFORM_WEB)
static void
app_update_desktop_background_state(InbeApp *app)
{
    int backgrounded;
    double now;
    int elapsed_ms;

    if(app == NULL)
        return;

    backgrounded = (!IsWindowFocused() || IsWindowMinimized()) ? 1 : 0;
    now = GetTime();
    if(!backgrounded) {
        app->backgrounded = 0;
        app->desktop_background_last_time = 0.0;
        return;
    }

    if(!app->backgrounded || app->desktop_background_last_time <= 0.0)
        app->desktop_background_last_time = now;
    elapsed_ms = (int)((now - app->desktop_background_last_time) * 1000.0);
    app->backgrounded = 1;
    app->desktop_background_last_time = now;

    if(elapsed_ms > 0)
        practice_active_advance_elapsed(app, elapsed_ms);
}
#endif

static void
updateapp(InbeApp *app)
{
    int center_x = view_width / 2;
    int frame_view_height = view_height;
    int center_y;
    int hover = 0;
    int frame_screen = app->inbe.screen;
    int first_run_guide_active = 0;
    int habits_guide_active = 0;
    int profile_guide_active = 0;
    int practice_fullscreen_modal = 0;
    int global_modal_drawn = 0;
    int content_input_clip_active = 0;
    int bottom_input_reserved = 0;

#if !ANDROID_BUILD && !defined(PLATFORM_WEB)
    app_update_desktop_background_state(app);
#endif
    for(int i = 0; i < practice_count(); i++) {
        const PracticeDefinition *practice = practice_get(i);
        if(practice->update != NULL)
            practice->update(app);
    }
    if(app->practice_coming_soon_ticks > 0)
        app->practice_coming_soon_ticks--;
    practice_screen_prepare_first_run_guide(app);
    habits_screen_prepare_first_run_guide(app);
    profile_screen_prepare_first_run_guide(app);
    first_run_guide_active = practice_screen_first_run_guide_active(app);
    habits_guide_active = habits_screen_first_run_guide_active(app);
    profile_guide_active = profile_screen_first_run_guide_active(app);
    practice_fullscreen_modal =
        app->modal.active &&
        (app->modal.type == UIModalPracticeMusic ||
         app->modal.type == UIModalEditProgressiveStartSpeed);
    if(app->modal.active || first_run_guide_active || habits_guide_active ||
       profile_guide_active) {
        PushUIInputCapture((Rectangle){0, 0, (float)view_width, (float)view_height}, 0);
    }

    view_height = app_page_height(app, view_height);
    center_y = view_height / 2;
    bottom_input_reserved = app_content_bottom_reserved(app);
    if(bottom_input_reserved > 0 && bottom_input_reserved < view_height) {
        PushUIInputClip((Rectangle){0, 0, (float)view_width,
                                       (float)(view_height - bottom_input_reserved)});
        content_input_clip_active = 1;
    }

    if(IsKeyPressed(KEY_BACK)
#if !ANDROID_BUILD && !defined(PLATFORM_WEB)
       || (IsKeyPressed(KEY_BACKSPACE) &&
           ((app->inbe.screen == InbeScreenStart &&
             app->practice_tab != PRACTICE_TAB_PLAY) ||
            (app->show_session_return_button &&
             (app->inbe.screen == InbeScreenSession ||
              app->inbe.screen == InbeScreenMeditation ||
              app->inbe.screen == InbeScreenSunSalutation))))
#endif
       ) {
        if(first_run_guide_active || habits_guide_active || profile_guide_active) {
            app->tutorial_step = 0;
            practice_screen_prepare_first_run_guide(app);
            app->habits_guide_step = 0;
            habits_screen_prepare_first_run_guide(app);
            app->profile_guide_step = 0;
            profile_screen_prepare_first_run_guide(app);
        } else if(app->modal.active) {
            app_close_modal(app);
        } else {
            handle_back_button(app);
        }
    }

    if(app->inbe.screen == InbeScreenSettings) {
        if(settings_screen_draw(app))
            goto finish_frame;
        goto finish_frame;
    }

    if(app->inbe.screen == InbeScreenProfile) {
        if(profile_screen_draw(app))
            goto finish_frame;
        goto finish_frame;
    }

    if(app->inbe.screen == InbeScreenPet) {
        pet_screen_draw(app);
        goto finish_frame;
    }

    if(app->inbe.screen == InbeScreenLanguage) {
        language_screen_draw(app);
        goto finish_frame;
    }

    if(app->inbe.screen == InbeScreenHabits) {
        draw_habits_screen(app);
        goto finish_frame;
    }

    if(app->inbe.screen == InbeScreenHabitEdit) {
        draw_habit_edit_screen(app);
        goto finish_frame;
    }

    if(app->inbe.screen == InbeScreenHabitSessionEdit) {
        draw_habit_session_edit_screen(app);
        goto finish_frame;
    }

    if(app->inbe.screen == InbeScreenStart) {
        practice_update_circle_bounds(app, app_content_top_reserved(app),
                                      app_content_bottom_reserved(app));
    } else if(app->inbe.screen == InbeScreenSession) {
        practice_update_circle_bounds(app, GetUITitleBarHeight(), 84);
    }

    if(app->inbe.screen == InbeScreenSession)
        practice_draw_active_breathing(app, center_x, center_y);


    switch (app->inbe.screen) {
    case InbeScreenStart:
        {
            if(!practice_fullscreen_modal) {
                if(app->practice_tab == PRACTICE_TAB_MANUAL) {
                    manual_screen_draw(app);
                } else if(app->practice_tab == PRACTICE_TAB_CONFIG) {
                    const PracticeDefinition *practice = practice_get(app->exercise_type);
                    if(practice->draw_config != NULL)
                        practice->draw_config(app);
                } else {
                    practice_screen_draw_home(app);
                }
            }
        }
        // Skip drawing on the same frame modal opens to prevent click propagation
        if(app->modal.active && app->modal.type == UIModalMeditationSetup &&
           app->modal_input_block_frame != app->inbe.frame) {
            const PracticeDefinition *practice = practice_get(PRACTICE_MEDITATION);
            if(practice->draw_setup_modal != NULL)
                practice->draw_setup_modal(app);
        }
        break;

    case InbeScreenSession:
        practice_update_active_breathing(app, center_x, center_y, &hover);
        break;

    case InbeScreenMeditation:
        {
            const PracticeDefinition *practice = practice_get(PRACTICE_MEDITATION);
            if(practice->draw_active_session != NULL)
                practice->draw_active_session(app, center_x, center_y);
        }
        break;

    case InbeScreenSunSalutation:
        {
            const PracticeDefinition *practice = practice_get(PRACTICE_SUN_SALUTATION);
            if(practice->draw_active_session != NULL)
                practice->draw_active_session(app, center_x, center_y);
        }
        break;

    case InbeScreenResults:
        practice_draw_results(app, center_x, center_y, &hover);
        break;

    }

finish_frame:
    if(content_input_clip_active)
        PopUIInputClip();
    if(practice_fullscreen_modal) {
        draw_global_modal(app);
        global_modal_drawn = 1;
    }
    view_height = frame_view_height;
    app_draw_bottom_nav(app);
    practice_screen_draw_first_run_guide(app);
    habits_screen_draw_first_run_guide(app);
    profile_screen_draw_first_run_guide(app);
    if(!global_modal_drawn)
        draw_global_modal(app);
    app_flush_deferred_settings(app);
    app_observe_direct_screen_change(app, frame_screen);
#if !defined(PLATFORM_WEB)
    if(app->transition_mode == APP_TRANSITION_FADE) {
        DrawUITransitionFade(&app->screen_transition, view_width,
                                   app_page_height(app, view_height),
                                   GetThemeBackground());
    }
#endif
    app_advance_screen_transition(app);
    app->inbe.frame++;
}

void
app_update_draw(void *vapp, Rectangle viewport) {
    InbeApp *app = vapp;
    int full_width;
    int full_height;
    int content_x = 0;
    int content_w;

    if(app == 0 || viewport.width <= 0 || viewport.height <= 0)
        return;

    app_reload_graphics_resources(app);

    full_width = (int)viewport.width;
    full_height = (int)viewport.height;
    app_full_view_width = full_width;
    view_width = full_width;
    view_height = full_height;

    /* Update DPI cache */
    UpdateUIDPI(view_width, view_height);
    SetUIViewSize(view_width, view_height);

    InitUI(view_width, view_height, GetUIDPIScale());
    practice_update_circle_bounds(app, app_content_top_reserved(app),
                                  app_content_bottom_reserved(app));

    app->cursor_clickable = 0;
    app->cursor_disabled = 0;
    SetUICursorClickable(&app->cursor_clickable);
    SetUICursorDisabled(&app->cursor_disabled);
    app_device_preferences_update(app);
    app_refresh_theme(app);

    DrawRectangleRec(viewport, GetThemeBackground());

    content_w = full_width - content_x;
    if(content_w < 1)
        content_w = 1;
    view_width = content_w;
    view_height = full_height;
    SetUIViewSize(view_width, view_height);
    InitUI(view_width, view_height, GetUIDPIScale());
    app->camera = (Camera2D){0};
    app->camera.zoom = 1.0f;
    app->camera.offset.x = viewport.x + content_x;
    app->camera.offset.y = viewport.y;
    SetUIFrame(app->camera);

    BeginUIClip((int)viewport.x + content_x, (int)viewport.y, content_w, full_height);
        BeginMode2D(app->camera);
            DrawRectangle(0, 0, view_width, view_height, GetThemeBackground());
            updateapp(app);
        EndMode2D();
    EndUIClip();
    habits_flush_save(app);
    app_sync_pump(app);
}

void
app_unload_texture(Texture2D texture) {
    if (texture.id != 0) {
        UnloadTexture(texture);
    }
}

static void SafeUnloadSound(Sound sound) {
    if (sound.frameCount != 0) {
        UnloadSound(sound);
    }
}

void
app_destroy(void *vapp)
{
    InbeApp *app = vapp;
    if (app == NULL) return;

    if(app->settings_dirty || app->settings_save_delay_ticks > 0)
        save_settings(app);
    habits_flush_save(app);

    // Unload all icons
    UnloadAllUIIconTextures(app->icons);
    app_unload_texture(app->pet.egg);
    app_unload_texture(app->font_shapes_texture);
    unload_locale_font(app);

    SafeUnloadSound(app->breath_in_sound);
    SafeUnloadSound(app->breath_out_sound);
    SafeUnloadSound(app->bell_sound);
    for(int i = 0; i < SETTINGS_SPEED_MAX; i++) {
        SafeUnloadSound(app->breath_in_cue_sounds[i]);
        SafeUnloadSound(app->breath_out_cue_sounds[i]);
    }
    SafeUnloadSound(app->bell_cue_sound);
    for(int i = 0; i < practice_count(); i++) {
        const PracticeDefinition *practice = practice_get(i);
        if(practice->destroy != NULL)
            practice->destroy(app);
    }

    if (app->audio_ready) {
        CloseAudioDevice();
        app->audio_ready = 0;
    }

    free(app);
}

int
app_should_use_tab_bar(const InbeApp *app)
{
    if(app == NULL)
        return 0;

    return app->navigation_mode == NAV_MODE_TABBAR;
}
