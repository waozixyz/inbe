#ifndef SCREENS_ELIST_SCREEN_H
#define SCREENS_ELIST_SCREEN_H

#include <stddef.h>

enum {
    ELIST_LIST_MAX = 32,
    ELIST_ITEM_MAX = 256,
    ELIST_TITLE_SIZE = 128,
    ELIST_COMMENT_SIZE = 512,
};

typedef struct EListList {
    char id[37];
    char title[ELIST_TITLE_SIZE];
    int sort_order;
    long long deleted_at;
    long long updated_at;
} EListList;

typedef struct EListItem {
    char id[37];
    char list_id[37];
    char title[ELIST_TITLE_SIZE];
    char comment[ELIST_COMMENT_SIZE];
    int done;
    int sort_order;
    long long deleted_at;
    long long updated_at;
} EListItem;

typedef struct EListState {
    EListList lists[ELIST_LIST_MAX];
    EListItem items[ELIST_ITEM_MAX];
    int list_count;
    int item_count;
    int selected_list;
    int loaded;
    int scroll;
    int input_cursor;
    int input_focused;
    int editing_item;
    char input[ELIST_TITLE_SIZE];
} EListState;

struct InnerBreeze;

void draw_elist_screen(struct InnerBreeze *app);

#endif
