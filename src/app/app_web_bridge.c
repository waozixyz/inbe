#include "app.h"
#include "app_internal.h"
#include "sync_account.h"
#include "storage.h"
#include <string.h>

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

#if defined(PLATFORM_WEB)
static KsyncAccount web_test_source_account;
static KsyncAccount web_test_imported_account;
static KsyncAccount web_test_loaded_account;
static char web_test_account_body[KSYNC_ACCOUNT_EXPORT_TEXT_SIZE];
static uint8_t web_test_public_key[1312];
static uint8_t web_test_private_key[2560];
static int web_test_sync_key_import_status;

static void
web_test_bytes_to_hex(const uint8_t *bytes, size_t len, char *out, size_t out_size)
{
    static const char hex[] = "0123456789abcdef";

    if(bytes == NULL || out == NULL || out_size < len * 2 + 1)
        return;
    for(size_t i = 0; i < len; i++) {
        out[i * 2] = hex[bytes[i] >> 4];
        out[i * 2 + 1] = hex[bytes[i] & 0x0f];
    }
    out[len * 2] = '\0';
}

static void
web_test_make_sync_account(KsyncAccount *account)
{
    if(account == NULL)
        return;
    memset(account, 0, sizeof(*account));
    for(size_t i = 0; i < sizeof(web_test_public_key); i++)
        web_test_public_key[i] = (uint8_t)(i * 31U + 7U);
    for(size_t i = 0; i < sizeof(web_test_private_key); i++)
        web_test_private_key[i] = (uint8_t)(i * 17U + 3U);
    sync_sha256_hex(web_test_public_key, sizeof(web_test_public_key),
                    account->public_id);
    web_test_bytes_to_hex(web_test_public_key, sizeof(web_test_public_key),
                          account->public_key_hex,
                          sizeof(account->public_key_hex));
    web_test_bytes_to_hex(web_test_private_key, sizeof(web_test_private_key),
                          account->private_key_hex,
                          sizeof(account->private_key_hex));
}

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

EMSCRIPTEN_KEEPALIVE
void
app_web_launch_practice(int practice_id)
{
    InbeApp *app = get_global_inbe_app();
    const PracticeDefinition *practice;

    if(app == NULL)
        return;

    app->exercise_type = practice_clamp_id(practice_id);
    app->main_tab = APP_MAIN_TAB_PRACTICE;
    app->practice_tab = PRACTICE_TAB_PLAY;
    if(app->modal.active)
        app_close_modal(app);

    practice = practice_get(app->exercise_type);
    if(practice->start != NULL)
        practice->start(app);
}

EMSCRIPTEN_KEEPALIVE
void
app_web_test_save_onboarding_state(void)
{
    InbeApp *app = get_global_inbe_app();

    if(app == NULL)
        return;

    snprintf(app->language, sizeof(app->language), "%s", "es");
    app->language_selected = 1;
    app->tutorial_seen = 1;
    app->habits_guide_seen = 1;
    save_settings(app);
}

EMSCRIPTEN_KEEPALIVE
int
app_web_test_onboarding_state(void)
{
    InbeApp *app = get_global_inbe_app();

    if(app == NULL)
        return 0;

    return app->language_selected && strcmp(app->language, "es") == 0 &&
           app->tutorial_seen && app->habits_guide_seen;
}

EMSCRIPTEN_KEEPALIVE
int
app_web_test_sync_key_state(void)
{
    KsyncAccount *source = &web_test_source_account;
    KsyncAccount *loaded = &web_test_loaded_account;

    memset(loaded, 0, sizeof(*loaded));
    if(web_test_sync_key_import_status != 1)
        return web_test_sync_key_import_status;
    if(!HasKsyncAccountValues(source))
        return -10;
    if(!sync_account_load(loaded))
        return -11;
    if(strcmp(source->public_id, loaded->public_id) != 0)
        return -12;
    if(strcmp(source->private_key_hex, loaded->private_key_hex) != 0)
        return -13;
    return 1;
}

EMSCRIPTEN_KEEPALIVE
void
app_web_test_import_sync_key(void)
{
    KsyncAccount *source = &web_test_source_account;
    KsyncAccount *imported = &web_test_imported_account;
    char *body = web_test_account_body;

    web_test_sync_key_import_status = 0;
    data_init();
    memset(imported, 0, sizeof(*imported));
    memset(body, 0, sizeof(web_test_account_body));

    web_test_make_sync_account(source);
    if(!HasKsyncAccountValues(source)) {
        web_test_sync_key_import_status = -1;
        return;
    }
    if(!ExportKsyncAccountText(source, body, sizeof(web_test_account_body))) {
        web_test_sync_key_import_status = -2;
        return;
    }
    if(!ParseKsyncAccountText(body, imported)) {
        web_test_sync_key_import_status = -3;
        return;
    }
    storage_settings_begin_write();
    storage_set_setting_text("sync_public_id", imported->public_id);
    storage_set_setting_text("sync_public_key", imported->public_key_hex);
    storage_set_setting_text("sync_private_key", imported->private_key_hex);
    storage_set_setting_text("sync_account_alias", "");
    storage_settings_end_write();
    web_test_sync_key_import_status = 1;
}
#endif
