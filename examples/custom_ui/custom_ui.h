/**
 * @file      custom_ui.h
 * @brief     Shared declarations for the custom_ui scaffold.
 *
 * The scaffold keeps a deliberately small surface:
 *   custom_ui.ino  - board bring-up, LVGL <-> EPD/touch glue, setup()/loop()
 *   ui_scr_mrg.*   - screen stack manager (copied unchanged from examples/factory)
 *   screen_home.c  - the screens themselves; this is where your UI goes
 */
#ifndef __CUSTOM_UI_H__
#define __CUSTOM_UI_H__

#include "utilities.h"
#include "lvgl.h"
#include "ui_scr_mrg.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************************
 *                                 SCREEN IDS
 * *******************************************************************************/
#define SCREEN_HOME_ID   0
#define SCREEN_DETAIL_ID 1

/* Lifecycle descriptors, defined in screen_home.c */
extern scr_lifecycle_t screen_home;
extern scr_lifecycle_t screen_detail;

/*********************************************************************************
 *                            DISPLAY REFRESH CONTROL
 * *******************************************************************************/
/*
 * E-paper cannot repaint continuously. Two modes are available:
 *
 *   disp_full_refr()          - flash the whole panel on the next flush. Slow
 *                               (~1s) but clears ghosting. Use on screen changes.
 *   disp_partial_refr_for(ms) - allow fast partial updates for the next `ms`
 *                               milliseconds, then fall back to a full refresh.
 *                               Use for counters, status text, toggles.
 */
void disp_full_refr(void);
void disp_partial_refr_for(uint32_t duration_ms);

/* Entry point called once from setup() after LVGL is up. */
void custom_ui_entry(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*__CUSTOM_UI_H__*/
