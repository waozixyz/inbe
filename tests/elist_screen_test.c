#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../build/kryon/generated/src/screens/elist_screen.c"

int view_width = 768;
int view_height = 720;
static int focused_id;
static int save_succeeds = 1;
static int sync_calls;
static char saved_comment[ELIST_COMMENT_SIZE];

void PushInspectSource(const char *path, int line)
{
    (void)path;
    (void)line;
}

void PopInspectSource(void)
{
}

int Scale(int value)
{
    return value;
}

void KryonSetFocus(int id)
{
    focused_id = id;
}

int app_auto_sync(InnerBreeze *app)
{
    (void)app;
    sync_calls++;
    return 1;
}

int storage_elist_create_list(const char *title, char *out_id)
{
    assert(strcmp(title, "Shopping") == 0);
    if(save_succeeds) {
        snprintf(out_id, 37, "created-list");
    }
    return save_succeeds;
}

int storage_elist_load(void *value)
{
    EListState *state = value;
    state->list_count = 2;
    snprintf(state->lists[0].id, sizeof(state->lists[0].id), "old-list");
    snprintf(state->lists[1].id, sizeof(state->lists[1].id), "created-list");
    return 1;
}

int storage_elist_update_list(const char *id, const char *title, int order)
{
    (void)id;
    (void)title;
    (void)order;
    return save_succeeds;
}

int storage_elist_create_item(const char *list_id, const char *title,
                            const char *comment, char *out_id)
{
    (void)list_id;
    (void)title;
    snprintf(saved_comment, sizeof(saved_comment), "%s", comment);
    (void)out_id;
    return save_succeeds;
}

int storage_elist_update_item(const char *id, const char *title,
                            const char *comment, int done, int order)
{
    (void)id;
    (void)title;
    snprintf(saved_comment, sizeof(saved_comment), "%s", comment);
    (void)done;
    (void)order;
    return save_succeeds;
}

int main(void)
{
    InnerBreeze app = {0};
    Rectangle bounds = elist_content_bounds();
    assert(bounds.x == 16 && bounds.width == 736);
    view_width = 360;
    bounds = elist_content_bounds();
    assert(bounds.x == 16 && bounds.width == 328);

    elist_begin_input(&app, -2, "Shopping");
    assert(focused_id == 6501 && app.elist.input_focused);
    save_succeeds = 0;
    elist_commit_input(&app);
    assert(app.elist.input_error && app.elist.editing_item == -2);
    assert(strcmp(app.elist.input, "Shopping") == 0 && sync_calls == 0);
    save_succeeds = 1;
    elist_commit_input(&app);
    assert(!app.elist.input_error && app.elist.editing_item == -1);
    assert(app.elist.input[0] == '\0' && app.elist.selected_list == 1);
    assert(sync_calls == 1);
    elist_begin_input(&app, 0, "Editing task");
    elist_select_list(&app, -1);
    assert(app.elist.selected_list == 1 && app.elist.editing_item == 0);
    elist_select_list(&app, 1);
    assert(strcmp(app.elist.input, "Editing task") == 0);
    elist_select_list(&app, 0);
    assert(app.elist.selected_list == 0 && app.elist.editing_item == -1);
    assert(app.elist.input[0] == '\0');
    assert(!app.elist.input_focused && focused_id == 0 && sync_calls == 1);
    app.elist.item_count = 1;
    snprintf(app.elist.items[0].comment, sizeof(app.elist.items[0].comment), "Saved note");
    elist_begin_input(&app, 0, "Renamed task");
    elist_commit_input(&app);
    assert(strcmp(saved_comment, "Saved note") == 0);
    elist_begin_input(&app, -1, "New task");
    elist_commit_input(&app);
    assert(saved_comment[0] == '\0');
    app.elist.item_count = 1;
    app.elist.selected_list = 0;
    snprintf(app.elist.items[0].id, sizeof(app.elist.items[0].id), "task");
    snprintf(app.elist.items[0].list_id, sizeof(app.elist.items[0].list_id), "old-list");
    assert(elist_item_visible(&app.elist, 0, 10.0));
    int before_sync = sync_calls;
    save_succeeds = 0;
    assert(!elist_set_done(&app, 0, 1, 10.0));
    assert(!app.elist.items[0].done && sync_calls == before_sync);
    save_succeeds = 1;
    assert(elist_set_done(&app, 0, 1, 10.0));
    assert(app.elist.items[0].done && sync_calls == before_sync + 1);
    assert(elist_item_visible(&app.elist, 0, 10.2));
    assert(!elist_item_visible(&app.elist, 0, 10.5));
    app.elist.show_completed = 1;
    assert(elist_item_visible(&app.elist, 0, 11.0));
    assert(elist_set_done(&app, 0, 0, 11.0));
    app.elist.show_completed = 0;
    assert(elist_item_visible(&app.elist, 0, 12.0));
    app.elist.show_completed = 1;
    elist_select_list(&app, 1);
    assert(!app.elist.show_completed && !elist_item_visible(&app.elist, 0, 12.0));
    puts("Lists layout, editing, completion, archive visibility and restore tests passed");
    return 0;
}
