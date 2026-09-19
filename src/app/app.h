#ifndef APP_APP_H
#define APP_APP_H

#include "kryon.h"
#include "ui_swipe.h"
#include "platform.h"
#include "core/breath_engine.h"
#include "breaks/break_engine.h"
#include "app_fwd.h"
#include "runtime_assets.h"
#include "screens/habits_screen.h"
#include "src/screens/elist_types.h"
#include "screens/settings/settings_types.h"
#include "storage/sync_account.h"

#include "app/app_types.h"
#include "app/metrics.h"

/* Shared UI viewport extents, updated by the app frame loop. */
extern int view_width;
extern int view_height;

int app_locale_dropdown(int id, int x, int y, int w, int h,
                        int *selected_index);

UIPanelFrame AppModalFrame(int width, int height, const char *title,
                              int left_icon_type, int right_icon_type);
UIGuideResult AppGuideOverlay(GuideOverlayProps guide);
ProfileImagePickerResult AppProfileImagePickerModal(ProfileImagePickerProps modal);
int AppIconSliderPopup(IconSliderPopupProps popup);
IconRowResult AppBottomIconRow(IconRowRequest row);
void AppReorderHandle(int x, int y, int w, int h, int active);
void AppReorderPlaceholder(Rectangle bounds);

extern AppConfig config;
int app_register_style_pack_variant(const char *source, const char *label,
                                    ThemeColors colors, int style_index);

int app_draw_close_title_bar(InnerBreeze*app, const char *title, int height);
int app_scaffold_close_title(const char *title, int height, void *user_data);
int app_draw_close_dropdown_title_bar(InnerBreeze*app, TitleBarDropdown dropdown,
                                      int height);

void app_set_host_api(InnerBreeze*app, HostApi host);
void app_init(void *app);
void app_update_draw(void *app, Rectangle viewport);
void app_destroy(void *app);
InnerBreeze*get_global_app(void);
void set_global_app(InnerBreeze*app);
void app_switch_screen(InnerBreeze*app, int screen);
AppRoute app_current_route(const InnerBreeze*app);
void app_request_route(InnerBreeze*app, AppRoute route);
void app_switch_route(InnerBreeze*app, AppRoute route);
void app_leave_practice_config(InnerBreeze*app);
int app_content_top_reserved(const InnerBreeze*app);
int app_toolbar_height(void);
int app_auto_sync(InnerBreeze*app);
void app_request_social_refresh(InnerBreeze*app);
int app_social_refresh_loading(void);
int app_sync_loading(void);
void app_request_friend_send(InnerBreeze*app, const char *target);
void app_request_friend_accept(InnerBreeze*app, const char *request_id);
void app_request_friend_decline(InnerBreeze*app, const char *request_id);
void app_request_friend_remove(InnerBreeze*app, const char *friend_user_id);
int app_should_use_tab_bar(const InnerBreeze*app);
void app_play_breath_cue(InnerBreeze*app, int dir);
void app_play_bell_cue(InnerBreeze*app, float scale);
void app_play_sound(InnerBreeze*app, Sound sound, float scale);
void app_audio_ensure_ready(InnerBreeze*app);
int app_audio_reinitialize(InnerBreeze*app);
float app_audio_output_level(InnerBreeze*app);
int app_bell_cue_playing(InnerBreeze*app);
void app_audio_library_load(InnerBreeze*app);
void app_audio_library_save(const InnerBreeze*app);
void app_audio_reload_cue_sounds(InnerBreeze*app);


int app_audio_import_custom_sound_ex(InnerBreeze*app, int cue, const char *path, int *error_code);
int app_audio_import_custom_music_ex(InnerBreeze*app, const char *path, int *error_code);
int app_audio_import_custom_sound(InnerBreeze*app, int cue, const char *path);
int app_audio_import_custom_music(InnerBreeze*app, const char *path);
int app_audio_remove_custom_sound(InnerBreeze*app, int index);
int app_audio_remove_custom_music(InnerBreeze*app, int index);
int app_audio_music_count(const InnerBreeze*app);
const char *app_audio_music_label(const InnerBreeze*app, int index);
int app_audio_music_path(const InnerBreeze*app, int index, char *out, size_t out_size);
int app_audio_sound_file_valid(const char *path);
int app_audio_music_file_valid(const char *path);
const char *app_audio_cue_default_asset(int cue);
int app_audio_cue_path(InnerBreeze*app, int cue, char *out, size_t out_size);
void app_audio_music_sanitize_selection(InnerBreeze*app);
void app_unload_texture(Texture2D texture);
const char *app_donation_url(void);
const char *app_bitcoin_donation_address(void);
const char *app_monero_donation_address(void);
const char *app_bitcoin_wallet_url(void);
const char *app_monero_wallet_url(void);
const char *app_bitcoin_trocador_url(void);
const char *app_monero_trocador_url(void);
const char *app_bitcoin_donation_url(void);
const char *app_monero_donation_url(void);

void app_open_modal(InnerBreeze*app, UIModalType type);
void app_close_modal(InnerBreeze*app);
void app_block_current_click(InnerBreeze*app);
void app_request_desktop_close(InnerBreeze*app);
void app_request_desktop_quit(InnerBreeze*app);
AppClosePromptResult app_consume_close_prompt_result(InnerBreeze*app);
SessionExitModalResult app_draw_session_exit_modal(int can_save,
                                                   const char *save_message,
                                                   const char *discard_message);

int clampi(int x, int min, int max);
int int_from_count(const char src[4]);
void count_from_int(char dst[4], int value);
void app_reload_after_import(InnerBreeze*app, int reload_settings);
void update_preview_bounds(BreathSession *breathing, int content_w, int max_h);
void refresh_theme_colors(int theme_id, int dark_mode);
void refresh_locale_dependent_text(InnerBreeze*app);
void apply_system_language_selection(InnerBreeze*app, int save_now);
void apply_language_selection(InnerBreeze*app, int language_index, int save_now);
void app_accept_language_selection(InnerBreeze*app);
int exercise_manual_seen(InnerBreeze*app, int exercise_type);
void mark_exercise_manual_seen(InnerBreeze*app, int exercise_type);
void sync_habits_for_activity(InnerBreeze*app, int exercise_type);
void draw_breath_preview(BreathSession *breathing, int center_x, int center_y);
void app_prepare_session_results(InnerBreeze*app, int activity, int primary_value,
                                 int secondary_value, const char *detail,
                                 const int *round_values, int round_count,
                                 const char *saved_path);

#include "app_nav.h"
#include "app/app_settings.h"

int app_sync_failure_needs_action(int result);
double app_sync_retry_delay(int attempt);
void app_sync_record_result(int result);
const char *app_sync_status_key(void);

#endif
