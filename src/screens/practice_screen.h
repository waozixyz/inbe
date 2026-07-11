#ifndef PRACTICE_SCREEN_H
#define PRACTICE_SCREEN_H

#include "flint.h"
#include "app_fwd.h"

#define PRACTICE_CATEGORY_TAB_H 40
#define PRACTICE_CATEGORY_TOAST_TICKS 120
#define PRACTICE_CONFIG_TAB_LIST (-1)

typedef struct PracticeSubscreenLayout {
    int integrated;
    int title_h;
    int scroll_y;
    int scroll_h;
} PracticeSubscreenLayout;

typedef struct PracticeManualLayout {
    int integrated;
    int title_h;
    int nav_y;
    int nav_h;
    int content_y;
    int content_h;
} PracticeManualLayout;

int practice_activity_count_for_tab(int tab);
int practice_activity_for_tab(int tab, int index);
const char *practice_activity_label(int exercise);
int practice_visible_mask_all(void);
int practice_is_visible(const InbeApp *app, int exercise);
void practice_set_visible(InbeApp *app, int exercise, int visible);
void practice_clamp_activity_to_tab(InbeApp *app);
void practice_screen_open_tab(InbeApp *app, int tab);
int practice_screen_subscreen_integrated(const InbeApp *app, int modal_type);
void practice_screen_config_layout(InbeApp *app, int modal_type, int content_top_gap,
                                   PracticeSubscreenLayout *layout);
void practice_screen_manual_layout(InbeApp *app, int modal_type, int page_count,
                                   int content_top_gap, int content_bottom_gap,
                                   int min_content_h, PracticeManualLayout *layout);
int practice_screen_handle_config_title(InbeApp *app, const char *title, int modal_type,
                                        void (*leave_config)(InbeApp *app));
void practice_screen_draw_home(InbeApp *app);
void practice_screen_draw_top_bar(InbeApp *app, int draw_menu);
void practice_screen_draw_floating_actions(InbeApp *app);
void practice_screen_draw_modal(InbeApp *app);
int practice_screen_first_run_guide_active(const InbeApp *app);
void practice_screen_prepare_first_run_guide(InbeApp *app);
void practice_screen_draw_first_run_guide(InbeApp *app);

#endif
