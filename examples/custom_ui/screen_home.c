/**
 * @file      screen_home.c
 * @brief     The screens. This is the file you rewrite.
 *
 * Each screen is a scr_lifecycle_t with four callbacks:
 *   create(parent) - build the widget tree under `parent` (called once, lazily)
 *   entry()        - screen became visible: start timers, refresh data
 *   exit()         - screen was covered or replaced: stop timers
 *   destroy()      - widgets are being freed: drop your cached pointers
 *
 * The manager owns the lv_obj_t screen object itself. Never call lv_obj_del()
 * on `parent`, and null your static pointers in destroy() so a later entry()
 * cannot touch freed widgets.
 */

#include "custom_ui.h"

/*********************************************************************************
 *                                 HOME SCREEN
 * *******************************************************************************/
static lv_obj_t *home_counter_label = NULL;
static int       home_counter = 0;

static void home_counter_btn_cb(lv_event_t *e)
{
    LV_UNUSED(e);

    home_counter++;
    if (home_counter_label != NULL) {
        lv_label_set_text_fmt(home_counter_label, "Taps: %d", home_counter);
    }

    // Small text change: partial refresh is enough, and far faster than a
    // full panel flash. Window closes after 5s and we return to full refreshes.
    disp_partial_refr_for(5000);
}

static void home_open_detail_cb(lv_event_t *e)
{
    LV_UNUSED(e);

    // push() keeps the home screen on the stack so scr_mgr_pop() comes back here.
    scr_mgr_push(SCREEN_DETAIL_ID, false);
}

static void home_create(lv_obj_t *parent)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "custom_ui");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_26, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    lv_obj_t *subtitle = lv_label_create(parent);
    lv_label_set_text(subtitle, BOARD_NAME);
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 52);

    home_counter_label = lv_label_create(parent);
    lv_label_set_text_fmt(home_counter_label, "Taps: %d", home_counter);
    lv_obj_align(home_counter_label, LV_ALIGN_TOP_MID, 0, 96);

    lv_obj_t *tap_btn = lv_btn_create(parent);
    lv_obj_set_size(tap_btn, 160, 48);
    lv_obj_align(tap_btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(tap_btn, home_counter_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *tap_label = lv_label_create(tap_btn);
    lv_label_set_text(tap_label, "Partial refresh");
    lv_obj_center(tap_label);

    lv_obj_t *detail_btn = lv_btn_create(parent);
    lv_obj_set_size(detail_btn, 160, 48);
    lv_obj_align(detail_btn, LV_ALIGN_CENTER, 0, 64);
    lv_obj_add_event_cb(detail_btn, home_open_detail_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *detail_label = lv_label_create(detail_btn);
    lv_label_set_text(detail_label, "Open detail");
    lv_obj_center(detail_label);
}

static void home_entry(void)
{
    // Arriving at a new screen repaints everything: flash the panel to clear
    // whatever the previous screen left behind.
    disp_full_refr();
}

static void home_exit(void)
{
}

static void home_destroy(void)
{
    home_counter_label = NULL;
}

scr_lifecycle_t screen_home = {
    .create  = home_create,
    .entry   = home_entry,
    .exit    = home_exit,
    .destroy = home_destroy,
};

/*********************************************************************************
 *                                DETAIL SCREEN
 * *******************************************************************************/
static void detail_back_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    scr_mgr_pop(false);
}

static void detail_create(lv_obj_t *parent)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "Detail");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    lv_obj_t *body = lv_label_create(parent);
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(body, LCD_HOR_SIZE - 32);
    lv_label_set_text(body,
                      "Pushed onto the stack. Pop returns to home without "
                      "rebuilding it.");
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, 56);

    lv_obj_t *back_btn = lv_btn_create(parent);
    lv_obj_set_size(back_btn, 160, 48);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -32);
    lv_obj_add_event_cb(back_btn, detail_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_center(back_label);
}

static void detail_entry(void)
{
    disp_full_refr();
}

static void detail_exit(void)
{
}

static void detail_destroy(void)
{
}

scr_lifecycle_t screen_detail = {
    .create  = detail_create,
    .entry   = detail_entry,
    .exit    = detail_exit,
    .destroy = detail_destroy,
};

/*********************************************************************************
 *                                    ENTRY
 * *******************************************************************************/
void custom_ui_entry(void)
{
    // Mono theme: this panel is 1bpp, the default theme's greys render as noise.
    lv_disp_t *disp = lv_disp_get_default();
    disp->theme = lv_theme_mono_init(disp, false, LV_FONT_DEFAULT);

    // Slide animations are unusable on e-paper - every frame is a panel refresh.
    scr_mgr_set_anim(LV_SCR_LOAD_ANIM_NONE, LV_SCR_LOAD_ANIM_NONE, LV_SCR_LOAD_ANIM_NONE);

    scr_mgr_init();
    scr_mgr_register(SCREEN_HOME_ID,   &screen_home);
    scr_mgr_register(SCREEN_DETAIL_ID, &screen_detail);

    scr_mgr_switch(SCREEN_HOME_ID, false);
}
