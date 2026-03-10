/**
 * @file ui_clock.c
 * @brief Clock screen: full-screen large time display with CPU usage.
 */

#include "ui_screens.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>

/* Declare your converted FontAwesome font - adjust name to match what
   the LVGL font converter outputs, e.g. fa_microchip_16 */
LV_FONT_DECLARE(fa_microchip_16);

/* FontAwesome microchip unicode codepoint as a UTF-8 string */
#define FA_MICROCHIP "\xEF\x8B\x9B"   /* U+F2DB */

static lv_obj_t *s_lbl_clock_big;
static lv_obj_t *s_lbl_cpu_icon;
static lv_obj_t *s_lbl_cpu;

/* ---------- CPU usage ---------- */

static TaskStatus_t s_tasks[20];
static uint32_t s_prev_idle = 0;
static uint32_t s_prev_all  = 0;

static float get_total_cpu_usage(void) {
    uint32_t num_tasks = uxTaskGetNumberOfTasks();
    if (num_tasks > 20) num_tasks = 20;

    uxTaskGetSystemState(s_tasks, num_tasks, NULL);

    /* Sum runtimes across ALL tasks and idle tasks separately.
     * Using per-task counters (not the wall-clock total_runtime) means both
     * idle and total are in the same unit space across both cores. */
    uint32_t idle_runtime = 0;
    uint32_t all_runtime  = 0;
    for (uint32_t i = 0; i < num_tasks; i++) {
        all_runtime += s_tasks[i].ulRunTimeCounter;
        if (strncmp(s_tasks[i].pcTaskName, "IDLE", 4) == 0) {
            idle_runtime += s_tasks[i].ulRunTimeCounter;
        }
    }

    uint32_t delta_all  = all_runtime  - s_prev_all;
    uint32_t delta_idle = idle_runtime - s_prev_idle;
    s_prev_all  = all_runtime;
    s_prev_idle = idle_runtime;

    if (delta_all == 0) return 0.0f;
    float usage = (1.0f - (float)delta_idle / delta_all) * 100.0f;
    if (usage < 0.0f)   usage = 0.0f;
    if (usage > 100.0f) usage = 100.0f;
    return usage;
}

/* ---------- Build / update ---------- */

void ui_clock_build(lv_obj_t *tile) {
    // Big clock label
    s_lbl_clock_big = lv_label_create(tile);
    lv_label_set_text(s_lbl_clock_big, "--:--:--");
    lv_obj_set_style_text_font(s_lbl_clock_big, &lv_font_montserrat_40, LV_PART_MAIN);
    lv_obj_align(s_lbl_clock_big, LV_ALIGN_CENTER, 0, -20);

    // CPU % label (anchor first so we can align icon relative to it)
    s_lbl_cpu = lv_label_create(tile);
    lv_label_set_text(s_lbl_cpu, "CPU: --%");
    lv_obj_set_style_text_font(s_lbl_cpu, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(s_lbl_cpu, LV_ALIGN_CENTER, 10, 30);

    // CPU icon (FontAwesome microchip glyph) — placed just left of the label
    s_lbl_cpu_icon = lv_label_create(tile);
    lv_label_set_text(s_lbl_cpu_icon, FA_MICROCHIP);
    lv_obj_set_style_text_font(s_lbl_cpu_icon, &fa_microchip_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_lbl_cpu_icon, lv_color_hex(0x4A90D9), LV_PART_MAIN);
    lv_obj_align_to(s_lbl_cpu_icon, s_lbl_cpu, LV_ALIGN_OUT_LEFT_MID, -8, 0);
}

void ui_clock_set_time(const char *hhmmss) {
    lv_label_set_text(s_lbl_clock_big, hhmmss);
}

void ui_clock_update_cpu(void) {
    float cpu = get_total_cpu_usage();
    char buf[16];
    snprintf(buf, sizeof(buf), "CPU: %.1f%%", cpu);
    lv_label_set_text(s_lbl_cpu, buf);
}